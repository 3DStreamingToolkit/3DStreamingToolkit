/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PEER_CONNECTION_CLIENT_H_
#define WEBRTC_PEER_CONNECTION_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <functional>
#include <vector>

#include "webrtc/base/nethelpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"

#include "ssl_capable_socket.h"

typedef std::map<int, std::string> Peers;

struct PeerConnectionClientObserver
{
	virtual void OnSignedIn() = 0;  // Called when we're logged on.

	virtual void OnDisconnected() = 0;

	virtual void OnPeerConnected(int id, const std::string& name) = 0;

	virtual void OnPeerDisconnected(int peer_id) = 0;

	virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;

	virtual void OnMessageSent(int err) = 0;

	virtual void OnServerConnectionFailure() = 0;

protected:
	virtual ~PeerConnectionClientObserver() {}
};

class PeerConnectionClient : public sigslot::has_slots<>,
                             public rtc::MessageHandler
{
public:
	enum State
	{
		NOT_CONNECTED,
		RESOLVING,
		SIGNING_IN,
		CONNECTED,
		SIGNING_OUT_WAITING,
		SIGNING_OUT,
	};

	PeerConnectionClient();

	~PeerConnectionClient();

	int id() const;

	bool is_connected() const;

	const Peers& peers() const;

	void RegisterObserver(PeerConnectionClientObserver* callback);

	void Connect(const std::string& server, int port,
				 const std::string& client_name);

	bool SendToPeer(int peer_id, const std::string& message);

	bool SendHangUp(int peer_id);

	bool IsSendingMessage();

	bool SignOut();

	bool Shutdown();

	// implements the MessageHandler interface
	void OnMessage(rtc::Message* msg);

	const std::string& authorization_header() const;

	void SetAuthorizationHeader(const std::string& value);

	int heartbeat_ms() const;

	void SetHeartbeatMs(const int tickMs);

protected:
	void DoConnect();

	void Close();

	void InitSocketSignals();

	bool ConnectControlSocket();

	void OnConnect(rtc::AsyncSocket* socket);

	void OnHangingGetConnect(rtc::AsyncSocket* socket);

	void OnHeartbeatGetConnect(rtc::AsyncSocket* socket);

	void OnMessageFromPeer(int peer_id, const std::string& message);

	// Quick and dirty support for parsing HTTP header values.
	bool GetHeaderValue(const std::string& data, size_t eoh,
						const char* header_pattern, size_t* value);

	bool GetHeaderValue(const std::string& data, size_t eoh,
						const char* header_pattern, std::string* value);

	// Returns true if the whole response has been read.
	bool ReadIntoBuffer(rtc::AsyncSocket* socket, std::string* data,
						size_t* content_length);

	void OnRead(rtc::AsyncSocket* socket);

	void OnHangingGetRead(rtc::AsyncSocket* socket);

	void OnHeartbeatGetRead(rtc::AsyncSocket* socket);

	// Parses a single line entry in the form "<name>,<id>,<connected>"
	bool ParseEntry(const std::string& entry, std::string* name, int* id,
					bool* connected);

	int GetResponseStatus(const std::string& response);

	int ParseServerResponse(const std::string& response, size_t content_length,
							size_t* peer_id, size_t* eoh);

	void OnClose(rtc::AsyncSocket* socket, int err);

	void OnHeartbeatGetClose(rtc::AsyncSocket* socket, int err);

	void OnResolveResult(rtc::AsyncResolverInterface* resolver);

	std::string PrepareRequest(const std::string& method, const std::string& fragment, std::map<std::string, std::string> headers);

	std::vector<PeerConnectionClientObserver*> callbacks_;
	bool server_address_ssl_;
	rtc::SocketAddress server_address_;
	rtc::AsyncResolver* resolver_;
	rtc::Thread* signaling_thread_;
	std::unique_ptr<SslCapableSocket> control_socket_;
	std::unique_ptr<SslCapableSocket> hanging_get_;
	std::unique_ptr<SslCapableSocket> heartbeat_get_;
	std::string onconnect_data_;
	std::string control_data_;
	std::string notification_data_;
	std::string client_name_;
	std::string authorization_header_;
	Peers peers_;
	State state_;
	int my_id_;
	int heartbeat_tick_ms_;

	struct ScheduledPeerMessage
	{
		int peer;
		std::string message;

		ScheduledPeerMessage(int peer_id, const std::string& messageData) :
			peer(peer_id), message(messageData)
		{}
	};

	std::queue<ScheduledPeerMessage> scheduled_messages_;
};

#endif  // WEBRTC_PEER_CONNECTION_CLIENT_H_
