/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "webrtc/p2p/base/dtlstransportchannel.h"

#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/packettransportinternal.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/thread.h"

namespace cricket {

// We don't pull the RTP constants from rtputils.h, to avoid a layer violation.
static const size_t kDtlsRecordHeaderLen = 13;
static const size_t kMaxDtlsPacketLen = 2048;
static const size_t kMinRtpPacketLen = 12;

// Maximum number of pending packets in the queue. Packets are read immediately
// after they have been written, so a capacity of "1" is sufficient.
static const size_t kMaxPendingPackets = 1;

// Minimum and maximum values for the initial DTLS handshake timeout. We'll pick
// an initial timeout based on ICE RTT estimates, but clamp it to this range.
static const int kMinHandshakeTimeout = 50;
static const int kMaxHandshakeTimeout = 3000;

static bool IsDtlsPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}
static bool IsDtlsClientHelloPacket(const char* data, size_t len) {
  if (!IsDtlsPacket(data, len)) {
    return false;
  }
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return len > 17 && u[0] == 22 && u[13] == 1;
}
static bool IsRtpPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kMinRtpPacketLen && (u[0] & 0xC0) == 0x80);
}

StreamInterfaceChannel::StreamInterfaceChannel(
    IceTransportInternal* ice_transport)
    : ice_transport_(ice_transport),
      state_(rtc::SS_OPEN),
      packets_(kMaxPendingPackets, kMaxDtlsPacketLen) {}

rtc::StreamResult StreamInterfaceChannel::Read(void* buffer,
                                                     size_t buffer_len,
                                                     size_t* read,
                                                     int* error) {
  if (state_ == rtc::SS_CLOSED)
    return rtc::SR_EOS;
  if (state_ == rtc::SS_OPENING)
    return rtc::SR_BLOCK;

  if (!packets_.ReadFront(buffer, buffer_len, read)) {
    return rtc::SR_BLOCK;
  }

  return rtc::SR_SUCCESS;
}

rtc::StreamResult StreamInterfaceChannel::Write(const void* data,
                                                      size_t data_len,
                                                      size_t* written,
                                                      int* error) {
  // Always succeeds, since this is an unreliable transport anyway.
  // TODO(zhihuang): Should this block if ice_transport_'s temporarily
  // unwritable?
  rtc::PacketOptions packet_options;
  ice_transport_->SendPacket(static_cast<const char*>(data), data_len,
                             packet_options);
  if (written) {
    *written = data_len;
  }
  return rtc::SR_SUCCESS;
}

bool StreamInterfaceChannel::OnPacketReceived(const char* data, size_t size) {
  // We force a read event here to ensure that we don't overflow our queue.
  bool ret = packets_.WriteBack(data, size, NULL);
  RTC_CHECK(ret) << "Failed to write packet to queue.";
  if (ret) {
    SignalEvent(this, rtc::SE_READ, 0);
  }
  return ret;
}

void StreamInterfaceChannel::Close() {
  packets_.Clear();
  state_ = rtc::SS_CLOSED;
}

DtlsTransport::DtlsTransport(IceTransportInternal* ice_transport)
    : transport_name_(ice_transport->transport_name()),
      component_(ice_transport->component()),
      network_thread_(rtc::Thread::Current()),
      ice_transport_(ice_transport),
      downward_(NULL),
      ssl_role_(rtc::SSL_CLIENT),
      ssl_max_version_(rtc::SSL_PROTOCOL_DTLS_12) {
  ice_transport_->SignalWritableState.connect(this,
                                              &DtlsTransport::OnWritableState);
  ice_transport_->SignalReadPacket.connect(this, &DtlsTransport::OnReadPacket);
  ice_transport_->SignalSentPacket.connect(this, &DtlsTransport::OnSentPacket);
  ice_transport_->SignalReadyToSend.connect(this,
                                            &DtlsTransport::OnReadyToSend);
  ice_transport_->SignalReceivingState.connect(
      this, &DtlsTransport::OnReceivingState);
}

DtlsTransport::~DtlsTransport() {}

bool DtlsTransport::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  if (dtls_active_) {
    if (certificate == local_certificate_) {
      // This may happen during renegotiation.
      LOG_J(LS_INFO, this) << "Ignoring identical DTLS identity";
      return true;
    } else {
      LOG_J(LS_ERROR, this) << "Can't change DTLS local identity in this state";
      return false;
    }
  }

  if (certificate) {
    local_certificate_ = certificate;
    dtls_active_ = true;
  } else {
    LOG_J(LS_INFO, this) << "NULL DTLS identity supplied. Not doing DTLS";
  }

  return true;
}

rtc::scoped_refptr<rtc::RTCCertificate> DtlsTransport::GetLocalCertificate()
    const {
  return local_certificate_;
}

bool DtlsTransport::SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) {
  if (dtls_active_) {
    LOG(LS_ERROR) << "Not changing max. protocol version "
                  << "while DTLS is negotiating";
    return false;
  }

  ssl_max_version_ = version;
  return true;
}

bool DtlsTransport::SetSslRole(rtc::SSLRole role) {
  if (dtls_) {
    if (ssl_role_ != role) {
      LOG(LS_ERROR) << "SSL Role can't be reversed after the session is setup.";
      return false;
    }
    return true;
  }

  ssl_role_ = role;
  return true;
}

bool DtlsTransport::GetSslRole(rtc::SSLRole* role) const {
  *role = ssl_role_;
  return true;
}

bool DtlsTransport::GetSslCipherSuite(int* cipher) {
  if (dtls_state() != DTLS_TRANSPORT_CONNECTED) {
    return false;
  }

  return dtls_->GetSslCipherSuite(cipher);
}

bool DtlsTransport::SetRemoteFingerprint(const std::string& digest_alg,
                                         const uint8_t* digest,
                                         size_t digest_len) {
  rtc::Buffer remote_fingerprint_value(digest, digest_len);

  // Once we have the local certificate, the same remote fingerprint can be set
  // multiple times.
  if (dtls_active_ && remote_fingerprint_value_ == remote_fingerprint_value &&
      !digest_alg.empty()) {
    // This may happen during renegotiation.
    LOG_J(LS_INFO, this) << "Ignoring identical remote DTLS fingerprint";
    return true;
  }

  // If the other side doesn't support DTLS, turn off |dtls_active_|.
  if (digest_alg.empty()) {
    RTC_DCHECK(!digest_len);
    LOG_J(LS_INFO, this) << "Other side didn't support DTLS.";
    dtls_active_ = false;
    return true;
  }

  // Otherwise, we must have a local certificate before setting remote
  // fingerprint.
  if (!dtls_active_) {
    LOG_J(LS_ERROR, this) << "Can't set DTLS remote settings in this state.";
    return false;
  }

  // At this point we know we are doing DTLS
  bool fingerprint_changing = remote_fingerprint_value_.size() > 0u;
  remote_fingerprint_value_ = std::move(remote_fingerprint_value);
  remote_fingerprint_algorithm_ = digest_alg;

  if (dtls_ && !fingerprint_changing) {
    // This can occur if DTLS is set up before a remote fingerprint is
    // received. For instance, if we set up DTLS due to receiving an early
    // ClientHello.
    rtc::SSLPeerCertificateDigestError err;
    if (!dtls_->SetPeerCertificateDigest(
            remote_fingerprint_algorithm_,
            reinterpret_cast<unsigned char*>(remote_fingerprint_value_.data()),
            remote_fingerprint_value_.size(), &err)) {
      LOG_J(LS_ERROR, this) << "Couldn't set DTLS certificate digest.";
      set_dtls_state(DTLS_TRANSPORT_FAILED);
      // If the error is "verification failed", don't return false, because
      // this means the fingerprint was formatted correctly but didn't match
      // the certificate from the DTLS handshake. Thus the DTLS state should go
      // to "failed", but SetRemoteDescription shouldn't fail.
      return err == rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;
    }
    return true;
  }

  // If the fingerprint is changing, we'll tear down the DTLS association and
  // create a new one, resetting our state.
  if (dtls_ && fingerprint_changing) {
    dtls_.reset(nullptr);
    set_dtls_state(DTLS_TRANSPORT_NEW);
    set_writable(false);
  }

  if (!SetupDtls()) {
    set_dtls_state(DTLS_TRANSPORT_FAILED);
    return false;
  }

  return true;
}

std::unique_ptr<rtc::SSLCertificate> DtlsTransport::GetRemoteSSLCertificate()
    const {
  if (!dtls_) {
    return nullptr;
  }

  return dtls_->GetPeerCertificate();
}

bool DtlsTransport::SetupDtls() {
  StreamInterfaceChannel* downward = new StreamInterfaceChannel(ice_transport_);

  dtls_.reset(rtc::SSLStreamAdapter::Create(downward));
  if (!dtls_) {
    LOG_J(LS_ERROR, this) << "Failed to create DTLS adapter.";
    delete downward;
    return false;
  }

  downward_ = downward;

  dtls_->SetIdentity(local_certificate_->identity()->GetReference());
  dtls_->SetMode(rtc::SSL_MODE_DTLS);
  dtls_->SetMaxProtocolVersion(ssl_max_version_);
  dtls_->SetServerRole(ssl_role_);
  dtls_->SignalEvent.connect(this, &DtlsTransport::OnDtlsEvent);
  dtls_->SignalSSLHandshakeError.connect(this,
                                         &DtlsTransport::OnDtlsHandshakeError);
  if (remote_fingerprint_value_.size() &&
      !dtls_->SetPeerCertificateDigest(
          remote_fingerprint_algorithm_,
          reinterpret_cast<unsigned char*>(remote_fingerprint_value_.data()),
          remote_fingerprint_value_.size())) {
    LOG_J(LS_ERROR, this) << "Couldn't set DTLS certificate digest.";
    return false;
  }

  // Set up DTLS-SRTP, if it's been enabled.
  if (!srtp_ciphers_.empty()) {
    if (!dtls_->SetDtlsSrtpCryptoSuites(srtp_ciphers_)) {
      LOG_J(LS_ERROR, this) << "Couldn't set DTLS-SRTP ciphers.";
      return false;
    }
  } else {
    LOG_J(LS_INFO, this) << "Not using DTLS-SRTP.";
  }

  LOG_J(LS_INFO, this) << "DTLS setup complete.";

  // If the underlying ice_transport is already writable at this point, we may
  // be able to start DTLS right away.
  MaybeStartDtls();
  return true;
}

bool DtlsTransport::SetSrtpCryptoSuites(const std::vector<int>& ciphers) {
  if (srtp_ciphers_ == ciphers)
    return true;

  if (dtls_state() == DTLS_TRANSPORT_CONNECTING) {
    LOG(LS_WARNING) << "Ignoring new SRTP ciphers while DTLS is negotiating";
    return true;
  }

  if (dtls_state() == DTLS_TRANSPORT_CONNECTED) {
    // We don't support DTLS renegotiation currently. If new set of srtp ciphers
    // are different than what's being used currently, we will not use it.
    // So for now, let's be happy (or sad) with a warning message.
    int current_srtp_cipher;
    if (!dtls_->GetDtlsSrtpCryptoSuite(&current_srtp_cipher)) {
      LOG(LS_ERROR)
          << "Failed to get the current SRTP cipher for DTLS transport";
      return false;
    }
    const std::vector<int>::const_iterator iter =
        std::find(ciphers.begin(), ciphers.end(), current_srtp_cipher);
    if (iter == ciphers.end()) {
      std::string requested_str;
      for (size_t i = 0; i < ciphers.size(); ++i) {
        requested_str.append(" ");
        requested_str.append(rtc::SrtpCryptoSuiteToName(ciphers[i]));
        requested_str.append(" ");
      }
      LOG(LS_WARNING) << "Ignoring new set of SRTP ciphers, as DTLS "
                      << "renegotiation is not supported currently "
                      << "current cipher = " << current_srtp_cipher << " and "
                      << "requested = " << "[" << requested_str << "]";
    }
    return true;
  }

  if (dtls_state() != DTLS_TRANSPORT_NEW) {
    LOG(LS_ERROR) << "Can't set SRTP ciphers for a closed session";
    return false;
  }

  srtp_ciphers_ = ciphers;
  return true;
}

bool DtlsTransport::GetSrtpCryptoSuite(int* cipher) {
  if (dtls_state() != DTLS_TRANSPORT_CONNECTED) {
    return false;
  }

  return dtls_->GetDtlsSrtpCryptoSuite(cipher);
}


// Called from upper layers to send a media packet.
int DtlsTransport::SendPacket(const char* data,
                              size_t size,
                              const rtc::PacketOptions& options,
                              int flags) {
  if (!dtls_active_) {
    // Not doing DTLS.
    return ice_transport_->SendPacket(data, size, options);
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      // Can't send data until the connection is active.
      // TODO(ekr@rtfm.com): assert here if dtls_ is NULL?
      return -1;
    case DTLS_TRANSPORT_CONNECTING:
      // Can't send data until the connection is active.
      return -1;
    case DTLS_TRANSPORT_CONNECTED:
      if (flags & PF_SRTP_BYPASS) {
        RTC_DCHECK(!srtp_ciphers_.empty());
        if (!IsRtpPacket(data, size)) {
          return -1;
        }

        return ice_transport_->SendPacket(data, size, options);
      } else {
        return (dtls_->WriteAll(data, size, NULL, NULL) == rtc::SR_SUCCESS)
                   ? static_cast<int>(size)
                   : -1;
      }
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // Can't send anything when we're closed.
      return -1;
    default:
      RTC_NOTREACHED();
      return -1;
  }
}

bool DtlsTransport::IsDtlsConnected() {
  return dtls_ && dtls_->IsTlsConnected();
}

// The state transition logic here is as follows:
// (1) If we're not doing DTLS-SRTP, then the state is just the
//     state of the underlying impl()
// (2) If we're doing DTLS-SRTP:
//     - Prior to the DTLS handshake, the state is neither receiving nor
//       writable
//     - When the impl goes writable for the first time we
//       start the DTLS handshake
//     - Once the DTLS handshake completes, the state is that of the
//       impl again
void DtlsTransport::OnWritableState(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(transport == ice_transport_);
  LOG_J(LS_VERBOSE, this)
      << "DTLSTransportChannelWrapper: ice_transport writable state changed to "
      << ice_transport_->writable();

  if (!dtls_active_) {
    // Not doing DTLS.
    // Note: SignalWritableState fired by set_writable.
    set_writable(ice_transport_->writable());
    return;
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      MaybeStartDtls();
      break;
    case DTLS_TRANSPORT_CONNECTED:
      // Note: SignalWritableState fired by set_writable.
      set_writable(ice_transport_->writable());
      break;
    case DTLS_TRANSPORT_CONNECTING:
      // Do nothing.
      break;
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // Should not happen. Do nothing.
      break;
  }
}

void DtlsTransport::OnReceivingState(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(transport == ice_transport_);
  LOG_J(LS_VERBOSE, this) << "DTLSTransportChannelWrapper: ice_transport "
                             "receiving state changed to "
                          << ice_transport_->receiving();
  if (!dtls_active_ || dtls_state() == DTLS_TRANSPORT_CONNECTED) {
    // Note: SignalReceivingState fired by set_receiving.
    set_receiving(ice_transport_->receiving());
  }
}

void DtlsTransport::OnReadPacket(rtc::PacketTransportInternal* transport,
                                 const char* data,
                                 size_t size,
                                 const rtc::PacketTime& packet_time,
                                 int flags) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_DCHECK(flags == 0);

  if (!dtls_active_) {
    // Not doing DTLS.
    SignalReadPacket(this, data, size, packet_time, 0);
    return;
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      if (dtls_) {
        LOG_J(LS_INFO, this) << "Packet received before DTLS started.";
      } else {
        LOG_J(LS_WARNING, this) << "Packet received before we know if we are "
                                << "doing DTLS or not.";
      }
      // Cache a client hello packet received before DTLS has actually started.
      if (IsDtlsClientHelloPacket(data, size)) {
        LOG_J(LS_INFO, this) << "Caching DTLS ClientHello packet until DTLS is "
                             << "started.";
        cached_client_hello_.SetData(data, size);
        // If we haven't started setting up DTLS yet (because we don't have a
        // remote fingerprint/role), we can use the client hello as a clue that
        // the peer has chosen the client role, and proceed with the handshake.
        // The fingerprint will be verified when it's set.
        if (!dtls_ && local_certificate_) {
          SetSslRole(rtc::SSL_SERVER);
          SetupDtls();
        }
      } else {
        LOG_J(LS_INFO, this) << "Not a DTLS ClientHello packet; dropping.";
      }
      break;

    case DTLS_TRANSPORT_CONNECTING:
    case DTLS_TRANSPORT_CONNECTED:
      // We should only get DTLS or SRTP packets; STUN's already been demuxed.
      // Is this potentially a DTLS packet?
      if (IsDtlsPacket(data, size)) {
        if (!HandleDtlsPacket(data, size)) {
          LOG_J(LS_ERROR, this) << "Failed to handle DTLS packet.";
          return;
        }
      } else {
        // Not a DTLS packet; our handshake should be complete by now.
        if (dtls_state() != DTLS_TRANSPORT_CONNECTED) {
          LOG_J(LS_ERROR, this) << "Received non-DTLS packet before DTLS "
                                << "complete.";
          return;
        }

        // And it had better be a SRTP packet.
        if (!IsRtpPacket(data, size)) {
          LOG_J(LS_ERROR, this) << "Received unexpected non-DTLS packet.";
          return;
        }

        // Sanity check.
        RTC_DCHECK(!srtp_ciphers_.empty());

        // Signal this upwards as a bypass packet.
        SignalReadPacket(this, data, size, packet_time, PF_SRTP_BYPASS);
      }
      break;
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // This shouldn't be happening. Drop the packet.
      break;
  }
}

void DtlsTransport::OnSentPacket(rtc::PacketTransportInternal* transport,
                                 const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);

  SignalSentPacket(this, sent_packet);
}

void DtlsTransport::OnReadyToSend(rtc::PacketTransportInternal* transport) {
  if (writable()) {
    SignalReadyToSend(this);
  }
}

void DtlsTransport::OnDtlsEvent(rtc::StreamInterface* dtls, int sig, int err) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(dtls == dtls_.get());
  if (sig & rtc::SE_OPEN) {
    // This is the first time.
    LOG_J(LS_INFO, this) << "DTLS handshake complete.";
    if (dtls_->GetState() == rtc::SS_OPEN) {
      // The check for OPEN shouldn't be necessary but let's make
      // sure we don't accidentally frob the state if it's closed.
      set_dtls_state(DTLS_TRANSPORT_CONNECTED);
      set_writable(true);
    }
  }
  if (sig & rtc::SE_READ) {
    char buf[kMaxDtlsPacketLen];
    size_t read;
    int read_error;
    rtc::StreamResult ret = dtls_->Read(buf, sizeof(buf), &read, &read_error);
    if (ret == rtc::SR_SUCCESS) {
      SignalReadPacket(this, buf, read, rtc::CreatePacketTime(0), 0);
    } else if (ret == rtc::SR_EOS) {
      // Remote peer shut down the association with no error.
      LOG_J(LS_INFO, this) << "DTLS transport closed";
      set_writable(false);
      set_dtls_state(DTLS_TRANSPORT_CLOSED);
    } else if (ret == rtc::SR_ERROR) {
      // Remote peer shut down the association with an error.
      LOG_J(LS_INFO, this) << "DTLS transport error, code=" << read_error;
      set_writable(false);
      set_dtls_state(DTLS_TRANSPORT_FAILED);
    }
  }
  if (sig & rtc::SE_CLOSE) {
    RTC_DCHECK(sig == rtc::SE_CLOSE);  // SE_CLOSE should be by itself.
    set_writable(false);
    if (!err) {
      LOG_J(LS_INFO, this) << "DTLS transport closed";
      set_dtls_state(DTLS_TRANSPORT_CLOSED);
    } else {
      LOG_J(LS_INFO, this) << "DTLS transport error, code=" << err;
      set_dtls_state(DTLS_TRANSPORT_FAILED);
    }
  }
}

void DtlsTransport::MaybeStartDtls() {
  if (dtls_ && ice_transport_->writable()) {
    ConfigureHandshakeTimeout();

    if (dtls_->StartSSL()) {
      // This should never fail:
      // Because we are operating in a nonblocking mode and all
      // incoming packets come in via OnReadPacket(), which rejects
      // packets in this state, the incoming queue must be empty. We
      // ignore write errors, thus any errors must be because of
      // configuration and therefore are our fault.
      RTC_NOTREACHED() << "StartSSL failed.";
      LOG_J(LS_ERROR, this) << "Couldn't start DTLS handshake";
      set_dtls_state(DTLS_TRANSPORT_FAILED);
      return;
    }
    LOG_J(LS_INFO, this) << "DtlsTransport: Started DTLS handshake";
    set_dtls_state(DTLS_TRANSPORT_CONNECTING);
    // Now that the handshake has started, we can process a cached ClientHello
    // (if one exists).
    if (cached_client_hello_.size()) {
      if (ssl_role_ == rtc::SSL_SERVER) {
        LOG_J(LS_INFO, this) << "Handling cached DTLS ClientHello packet.";
        if (!HandleDtlsPacket(cached_client_hello_.data<char>(),
                              cached_client_hello_.size())) {
          LOG_J(LS_ERROR, this) << "Failed to handle DTLS packet.";
        }
      } else {
        LOG_J(LS_WARNING, this) << "Discarding cached DTLS ClientHello packet "
                                << "because we don't have the server role.";
      }
      cached_client_hello_.Clear();
    }
  }
}

// Called from OnReadPacket when a DTLS packet is received.
bool DtlsTransport::HandleDtlsPacket(const char* data, size_t size) {
  // Sanity check we're not passing junk that
  // just looks like DTLS.
  const uint8_t* tmp_data = reinterpret_cast<const uint8_t*>(data);
  size_t tmp_size = size;
  while (tmp_size > 0) {
    if (tmp_size < kDtlsRecordHeaderLen)
      return false;  // Too short for the header

    size_t record_len = (tmp_data[11] << 8) | (tmp_data[12]);
    if ((record_len + kDtlsRecordHeaderLen) > tmp_size)
      return false;  // Body too short

    tmp_data += record_len + kDtlsRecordHeaderLen;
    tmp_size -= record_len + kDtlsRecordHeaderLen;
  }

  // Looks good. Pass to the SIC which ends up being passed to
  // the DTLS stack.
  return downward_->OnPacketReceived(data, size);
}

void DtlsTransport::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

void DtlsTransport::set_writable(bool writable) {
  if (writable_ == writable) {
    return;
  }
  LOG_J(LS_VERBOSE, this) << "set_writable from:" << writable_ << " to "
                          << writable;
  writable_ = writable;
  if (writable_) {
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

void DtlsTransport::set_dtls_state(DtlsTransportState state) {
  if (dtls_state_ == state) {
    return;
  }
  LOG_J(LS_VERBOSE, this) << "set_dtls_state from:" << dtls_state_ << " to "
                          << state;
  dtls_state_ = state;
  SignalDtlsState(this, state);
}

void DtlsTransport::OnDtlsHandshakeError(rtc::SSLHandshakeError error) {
  SignalDtlsHandshakeError(error);
}

void DtlsTransport::ConfigureHandshakeTimeout() {
  RTC_DCHECK(dtls_);
  rtc::Optional<int> rtt = ice_transport_->GetRttEstimate();
  if (rtt) {
    // Limit the timeout to a reasonable range in case the ICE RTT takes
    // extreme values.
    int initial_timeout = std::max(kMinHandshakeTimeout,
                                   std::min(kMaxHandshakeTimeout,
                                            2 * (*rtt)));
    LOG_J(LS_INFO, this) << "configuring DTLS handshake timeout "
                         << initial_timeout << " based on ICE RTT " << *rtt;

    dtls_->SetInitialRetransmissionTimeout(initial_timeout);
  } else {
    LOG_J(LS_INFO, this)
        << "no RTT estimate - using default DTLS handshake timeout";
  }
}


}  // namespace cricket
