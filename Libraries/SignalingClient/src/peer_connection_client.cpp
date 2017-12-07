/*
*  Copyright 2012 The WebRTC Project Authors. All rights reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "peer_connection_client.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/stringutils.h"

#ifdef WIN32
#include "webrtc/base/win32socketserver.h"
#endif

using rtc::sprintfn;

namespace
{
	// This is our magical hangup signal.
	const char kByeMessage[] = "BYE";

	// Delay between server connection retries, in milliseconds
	const int kReconnectDelay = 2000;

	// The message id we use when scheduling a heartbeat operation
	const int kHeartbeatScheduleId = 1523U;

	// The default value for the tick heartbeat, used to disable the heartbeat
	const int kHeartbeatDefault = -1;
}

PeerConnectionClient::PeerConnectionClient() :
	resolver_(NULL),
	state_(NOT_CONNECTED),
	my_id_(-1),
	heartbeat_tick_ms_(kHeartbeatDefault),
	server_address_ssl_(false)
{
	// use the current thread or wrap a thread for signaling_thread_
	auto thread = rtc::Thread::Current();

	signaling_thread_ = thread == nullptr ?
		rtc::ThreadManager::Instance()->WrapCurrentThread() : thread;
}

PeerConnectionClient::~PeerConnectionClient()
{
}

void PeerConnectionClient::InitSocketSignals()
{
	RTC_DCHECK(control_socket_.get() != NULL);
	RTC_DCHECK(hanging_get_.get() != NULL);
	control_socket_->SignalCloseEvent.connect(this, &PeerConnectionClient::OnClose);
	hanging_get_->SignalCloseEvent.connect(this, &PeerConnectionClient::OnClose);
	heartbeat_get_->SignalCloseEvent.connect(this, &PeerConnectionClient::OnHeartbeatGetClose);

	control_socket_->SignalConnectEvent.connect(this, &PeerConnectionClient::OnConnect);
	hanging_get_->SignalConnectEvent.connect(this, &PeerConnectionClient::OnHangingGetConnect);
	heartbeat_get_->SignalConnectEvent.connect(this, &PeerConnectionClient::OnHeartbeatGetConnect);

	control_socket_->SignalReadEvent.connect(this, &PeerConnectionClient::OnRead);
	hanging_get_->SignalReadEvent.connect(this, &PeerConnectionClient::OnHangingGetRead);
	heartbeat_get_->SignalReadEvent.connect(this, &PeerConnectionClient::OnHeartbeatGetRead);
}

int PeerConnectionClient::id() const
{
	return my_id_;
}

bool PeerConnectionClient::is_connected() const
{
	return my_id_ != -1;
}

const Peers& PeerConnectionClient::peers() const
{
	return peers_;
}

void PeerConnectionClient::RegisterObserver(PeerConnectionClientObserver* callback)
{
	;
	callbacks_.push_back(callback);
}

void PeerConnectionClient::Connect(const std::string& server, int port,
	const std::string& client_name)
{
	RTC_DCHECK(!server.empty());
	RTC_DCHECK(!client_name.empty());

	if (state_ != NOT_CONNECTED)
	{
		LOG(WARNING) << "The client must not be connected before you can call Connect()";
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnServerConnectionFailure(); });
		return;
	}

	if (server.empty() || client_name.empty())
	{
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnServerConnectionFailure(); });
		return;
	}

	if (port <= 0)
	{
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnServerConnectionFailure(); });
		return;
	}

	std::string parsedServer = server;

	if (parsedServer.substr(0, 8).compare("https://") == 0)
	{
		server_address_ssl_ = true;
		parsedServer = parsedServer.substr(8);
	}
	else if (parsedServer.substr(0, 7).compare("http://") == 0)
	{
		parsedServer = parsedServer.substr(7);
	}

	server_address_.SetIP(parsedServer);
	server_address_.SetPort(port);
	client_name_ = client_name;
	std::replace(client_name_.begin(), client_name_.end(), ' ', '-');

	if (server_address_.IsUnresolvedIP())
	{
		state_ = RESOLVING;
		resolver_ = new rtc::AsyncResolver();
		resolver_->SignalDone.connect(this, &PeerConnectionClient::OnResolveResult);
		resolver_->Start(server_address_);
	}
	else
	{
		DoConnect();
	}
}

void PeerConnectionClient::OnResolveResult(rtc::AsyncResolverInterface* resolver)
{
	if (resolver_->GetError() != 0)
	{
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnServerConnectionFailure(); });
		resolver_->Destroy(false);
		resolver_ = NULL;
		state_ = NOT_CONNECTED;
	}
	else
	{
		server_address_ = resolver_->address();
		DoConnect();
	}
}

std::string PeerConnectionClient::PrepareRequest(const std::string& method, const std::string& fragment, std::map<std::string, std::string> headers)
{
	std::string result;

	for (size_t i = 0; i < method.length(); ++i)
	{
		result += (char)toupper(method[i]);
	}

	result += " " + fragment + " HTTP/1.0\r\n";

	for (auto it = headers.begin(); it != headers.end(); ++it)
	{
		result += it->first + ": " + it->second + "\r\n";
	}

	if (!authorization_header_.empty())
	{
		result += "Authorization: " + authorization_header_ + "\r\n";
	}

	result += "\r\n";

	return result;
}

void PeerConnectionClient::DoConnect()
{
	control_socket_.reset(new SslCapableSocket(server_address_.ipaddr().family(), server_address_ssl_, signaling_thread_));
	hanging_get_.reset(new SslCapableSocket(server_address_.ipaddr().family(), server_address_ssl_, signaling_thread_));
	heartbeat_get_.reset(new SslCapableSocket(server_address_.ipaddr().family(), server_address_ssl_, signaling_thread_));

	InitSocketSignals();
	std::string clientName = client_name_;
	std::string hostName = server_address_.hostname();

	onconnect_data_ = PrepareRequest("GET", "/sign_in?peer_name=" + clientName, { { "Host", hostName } });

	bool ret = ConnectControlSocket();
	if (ret)
	{
		state_ = SIGNING_IN;
	}

	if (!ret)
	{
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnServerConnectionFailure(); });
	}
}

bool PeerConnectionClient::SendToPeer(int peer_id, const std::string& message)
{
	if (state_ != CONNECTED)
	{
		return false;
	}

	RTC_DCHECK(is_connected());
	if (control_socket_->GetState() != rtc::Socket::CS_CLOSED)
	{
		return false;
	}

	if (!is_connected() || peer_id == -1)
	{
		return false;
	}

	onconnect_data_ = PrepareRequest("POST",
		"/message?peer_id=" + std::to_string(my_id_) + "&to=" + std::to_string(peer_id),
		{
			{ "Host", server_address_.hostname() },
			{ "Content-Length", std::to_string(message.length()) },
			{ "Content-Type", "text/plain" }
		});

	onconnect_data_ += message;
	return ConnectControlSocket();
}

bool PeerConnectionClient::SendHangUp(int peer_id)
{
	return SendToPeer(peer_id, kByeMessage);
}

bool PeerConnectionClient::IsSendingMessage()
{
	return state_ == CONNECTED && control_socket_->GetState() != rtc::Socket::CS_CLOSED;
}

bool PeerConnectionClient::SignOut()
{
	if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
	{
		return true;
	}

	if (hanging_get_->GetState() != rtc::Socket::CS_CLOSED)
	{
		hanging_get_->Close();
	}

	if (control_socket_->GetState() == rtc::Socket::CS_CLOSED)
	{
		state_ = SIGNING_OUT;

		if (my_id_ != -1)
		{
			onconnect_data_ = PrepareRequest("GET", "/sign_out?peer_id=" + std::to_string(my_id_), { { "Host", server_address_.hostname() } });
			return ConnectControlSocket();
		}
		else
		{
			// Can occur if the app is closed before we finish connecting.
			return true;
		}
	}
	else
	{
		state_ = SIGNING_OUT_WAITING;
	}

	return true;
}

bool PeerConnectionClient::Shutdown()
{
	if (heartbeat_get_.get() != nullptr)
	{
		heartbeat_get_->Close();
	}

	if (hanging_get_.get() != nullptr)
	{
		hanging_get_->Close();
	}

	if (control_socket_.get() != nullptr)
	{
		control_socket_->Close();
	}

	state_ = NOT_CONNECTED;

	return true;
}


void PeerConnectionClient::Close()
{
	control_socket_->Close();
	hanging_get_->Close();
	onconnect_data_.clear();
	peers_.clear();
	if (resolver_ != NULL)
	{
		resolver_->Destroy(false);
		resolver_ = NULL;
	}

	my_id_ = -1;
	state_ = NOT_CONNECTED;
}

bool PeerConnectionClient::ConnectControlSocket()
{
	RTC_DCHECK(control_socket_->GetState() == rtc::Socket::CS_CLOSED);
	int err = control_socket_->Connect(server_address_);
	if (err == SOCKET_ERROR)
	{
		Close();
		return false;
	}

	return true;
}

void PeerConnectionClient::OnConnect(rtc::AsyncSocket* socket)
{
	RTC_DCHECK(!onconnect_data_.empty());
	size_t sent = socket->Send(onconnect_data_.c_str(), onconnect_data_.length());
	RTC_DCHECK(sent == onconnect_data_.length());
	onconnect_data_.clear();
}

void PeerConnectionClient::OnHangingGetConnect(rtc::AsyncSocket* socket)
{
	auto req = PrepareRequest("GET", "/wait?peer_id=" + std::to_string(my_id_), { { "Host", server_address_.hostname() } });

	int sent = socket->Send(req.c_str(), req.length());
	RTC_DCHECK(sent == req.length());
}

void PeerConnectionClient::OnMessageFromPeer(int peer_id, const std::string& message)
{
	if (message.length() == (sizeof(kByeMessage) - 1) &&
		message.compare(kByeMessage) == 0)
	{
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [&](PeerConnectionClientObserver* o) { o->OnPeerDisconnected(peer_id); });
	}
	else
	{
		std::for_each(callbacks_.rbegin(), callbacks_.rend(), [&](PeerConnectionClientObserver* o) { o->OnMessageFromPeer(peer_id, message); });
	}
}

bool PeerConnectionClient::GetHeaderValue(const std::string& data, size_t eoh,
	const char* header_pattern, size_t* value)
{
	RTC_DCHECK(value != NULL);
	size_t found = data.find(header_pattern);
	if (found != std::string::npos && found < eoh)
	{
		*value = atoi(&data[found + strlen(header_pattern)]);
		return true;
	}

	return false;
}

bool PeerConnectionClient::GetHeaderValue(const std::string& data, size_t eoh,
	const char* header_pattern, std::string* value)
{
	RTC_DCHECK(value != NULL);
	size_t found = data.find(header_pattern);
	if (found != std::string::npos && found < eoh)
	{
		size_t begin = found + strlen(header_pattern);
		size_t end = data.find("\r\n", begin);
		if (end == std::string::npos)
		{
			end = eoh;
		}

		value->assign(data.substr(begin, end - begin));
		return true;
	}

	return false;
}

bool PeerConnectionClient::ReadIntoBuffer(rtc::AsyncSocket* socket, std::string* data,
	size_t* content_length)
{
	char buffer[0xffff];
	do
	{
		int bytes = socket->Recv(buffer, sizeof(buffer), nullptr);
		if (bytes <= 0)
		{
			break;
		}

		data->append(buffer, bytes);
	} while (true);

	bool ret = false;
	size_t i = data->find("\r\n\r\n");
	if (i != std::string::npos)
	{
		LOG(INFO) << "Headers received";
		if (GetHeaderValue(*data, i, "\r\nContent-Length: ", content_length))
		{
			size_t total_response_size = (i + 4) + *content_length;
			if (data->length() >= total_response_size)
			{
				ret = true;
				std::string should_close;
				const char kConnection[] = "\r\nConnection: ";
				if (GetHeaderValue(*data, i, kConnection, &should_close) &&
					should_close.compare("close") == 0)
				{
					socket->Close();

					// Since we closed the socket, there was no notification delivered
					// to us.  Compensate by letting ourselves know.
					OnClose(socket, 0);
				}
			}
			else
			{
				// We haven't received everything.  Just continue to accept data.
			}
		}
		else
		{
			LOG(LS_ERROR) << "No content length field specified by the server.";
		}
	}

	return ret;
}

void PeerConnectionClient::OnRead(rtc::AsyncSocket* socket)
{
	size_t content_length = 0;
	if (ReadIntoBuffer(socket, &control_data_, &content_length))
	{
		size_t peer_id = 0, eoh = 0;
		int status = ParseServerResponse(control_data_, content_length, &peer_id, &eoh);
		if (status == 200)
		{
			if (my_id_ == -1)
			{
				// First response.  Let's store our server assigned ID.
				RTC_DCHECK(state_ == SIGNING_IN);
				my_id_ = static_cast<int>(peer_id);
				RTC_DCHECK(my_id_ != -1);

				// The body of the response will be a list of already connected peers.
				if (content_length)
				{
					size_t pos = eoh + 4;
					while (pos < control_data_.size())
					{
						size_t eol = control_data_.find('\n', pos);
						if (eol == std::string::npos)
						{
							break;
						}

						int id = 0;
						std::string name;
						bool connected;
						if (ParseEntry(control_data_.substr(pos, eol - pos),
							&name, &id, &connected) && id != my_id_)
						{
							peers_[id] = name;
							std::for_each(callbacks_.rbegin(), callbacks_.rend(), [&](PeerConnectionClientObserver* o) { o->OnPeerConnected(id, name); });
						}

						pos = eol + 1;
					}
				}

				RTC_DCHECK(is_connected());
				std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnSignedIn(); });
			}
			else if (state_ == SIGNING_OUT)
			{
				Close();
				std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnDisconnected(); });
			}
			else if (state_ == SIGNING_OUT_WAITING)
			{
				SignOut();
			}

			control_data_.clear();
		}
		else
		{
			LOG(LS_ERROR) << "Received error from server: " << std::to_string(status);

			// TODO(bengreenier): special case for azure 500 issue
			// see https://github.com/CatalystCode/3dtoolkit/issues/45
			if (status == 500)
			{
				control_socket_->Close();
				control_socket_->Connect(server_address_);
			}
			else
			{
				Close();
				std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnDisconnected(); });
				control_data_.clear();
			}
		}

		if (state_ == SIGNING_IN)
		{
			RTC_DCHECK(hanging_get_->GetState() == rtc::Socket::CS_CLOSED);
			state_ = CONNECTED;
			hanging_get_->Connect(server_address_);

			if (heartbeat_tick_ms_ != kHeartbeatDefault)
			{
				heartbeat_get_->Connect(server_address_);
			}
		}
	}
}

void PeerConnectionClient::OnHangingGetRead(rtc::AsyncSocket* socket)
{
	LOG(INFO) << __FUNCTION__;
	size_t content_length = 0;
	if (ReadIntoBuffer(socket, &notification_data_, &content_length))
	{
		size_t peer_id = 0, eoh = 0;
		int status = ParseServerResponse(notification_data_, content_length, &peer_id, &eoh);

		if (status == 200)
		{
			// Store the position where the body begins.
			size_t pos = eoh + 4;

			if (my_id_ == static_cast<int>(peer_id))
			{
				// A notification about a new member or a member that just
				// disconnected.
				int id = 0;
				std::string name;
				bool connected = false;
				if (ParseEntry(notification_data_.substr(pos), &name, &id, &connected))
				{
					if (connected)
					{
						peers_[id] = name;
						std::for_each(callbacks_.rbegin(), callbacks_.rend(), [&](PeerConnectionClientObserver* o) { o->OnPeerConnected(id, name); });
					}
					else
					{
						peers_.erase(id);
						std::for_each(callbacks_.rbegin(), callbacks_.rend(), [&](PeerConnectionClientObserver* o) { o->OnPeerDisconnected(id); });
					}
				}
			}
			else
			{
				OnMessageFromPeer(static_cast<int>(peer_id), notification_data_.substr(pos));
			}
		}
		else
		{
			LOG(LS_ERROR) << "Received error from server: " << std::to_string(status);

			// TODO(bengreenier): special case for azure 500 issue
			// see https://github.com/CatalystCode/3dtoolkit/issues/45
			if (status == 500)
			{
				hanging_get_->Close();
				hanging_get_->Connect(server_address_);
			}
			else
			{
				Close();
				std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnDisconnected(); });
			}
		}

		notification_data_.clear();
	}

	if (hanging_get_->GetState() == rtc::Socket::CS_CLOSED && state_ == CONNECTED)
	{
		hanging_get_->Connect(server_address_);
	}
}

bool PeerConnectionClient::ParseEntry(const std::string& entry, std::string* name,
	int* id, bool* connected)
{
	RTC_DCHECK(name != NULL);
	RTC_DCHECK(id != NULL);
	RTC_DCHECK(connected != NULL);
	RTC_DCHECK(!entry.empty());

	*connected = false;
	size_t separator = entry.find(',');
	if (separator != std::string::npos)
	{
		*id = atoi(&entry[separator + 1]);
		name->assign(entry.substr(0, separator));
		separator = entry.find(',', separator + 1);
		if (separator != std::string::npos)
		{
			*connected = atoi(&entry[separator + 1]) ? true : false;
		}
	}

	return !name->empty();
}

int PeerConnectionClient::GetResponseStatus(const std::string& response)
{
	int status = -1;
	size_t pos = response.find(' ');
	if (pos != std::string::npos)
	{
		status = atoi(&response[pos + 1]);
	}

	return status;
}

int PeerConnectionClient::ParseServerResponse(const std::string& response,
	size_t content_length, size_t* peer_id, size_t* eoh)
{
	int status = GetResponseStatus(response.c_str());

	if (status == 200)
	{

		*eoh = response.find("\r\n\r\n");
		RTC_DCHECK(*eoh != std::string::npos);
		if (*eoh == std::string::npos)
		{
			return -1;
		}

		*peer_id = -1;

		// See comment in peer_channel.cc for why we use the Pragma header and
		// not e.g. "X-Peer-Id".
		GetHeaderValue(response, *eoh, "\r\nPragma: ", peer_id);
	}

	return status;
}

void PeerConnectionClient::OnHeartbeatGetClose(rtc::AsyncSocket* socket, int err)
{
	// if we're still connected, schedule a reconnect
	if (state_ == State::CONNECTED)
	{
		LOG(INFO) << "heartbeat socket closed (" << err << "), scheduling reconnection attempt";

		rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, heartbeat_tick_ms_, this, kHeartbeatScheduleId);
	}
}

void PeerConnectionClient::OnClose(rtc::AsyncSocket* socket, int err)
{
	LOG(INFO) << __FUNCTION__;

	socket->Close();

#ifdef WIN32
	if (err != WSAECONNREFUSED)
	{
#else
	if (err != ECONNREFUSED)
	{
#endif
		if (socket == hanging_get_.get())
		{
			if (state_ == CONNECTED)
			{
				hanging_get_->Close();
				hanging_get_->Connect(server_address_);
			}
		}
		else
		{
			std::for_each(callbacks_.rbegin(), callbacks_.rend(), [&](PeerConnectionClientObserver* o) { o->OnMessageSent(err); });
		}
	}
	else
	{
		if (socket == control_socket_.get())
		{
			LOG(WARNING) << "Connection refused; retrying in 2 seconds";
			rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, kReconnectDelay, this, 0);
		}
		else
		{
			Close();
			std::for_each(callbacks_.rbegin(), callbacks_.rend(), [](PeerConnectionClientObserver* o) { o->OnDisconnected(); });
		}
	}
	}

void PeerConnectionClient::OnMessage(rtc::Message* msg)
{
	// indicates this message is to trigger a heartbeat request
	if (msg->message_id == kHeartbeatScheduleId)
	{
		// if we aren't connected any longer, don't beat
		if (state_ != State::CONNECTED)
		{
			return;
		}

		// if the socket is still open, close it and then reconnect to trigger the beat...
		if (heartbeat_get_->GetState() != rtc::Socket::ConnState::CS_CLOSED)
		{
			heartbeat_get_->Close();
		}

		heartbeat_get_->Connect(server_address_);
	}
	else
	{
		// default case - other than the heartbeat message, there is only one other message ("retry")
		DoConnect();
	}
}

const std::string& PeerConnectionClient::authorization_header() const
{
	return authorization_header_;
}

void PeerConnectionClient::SetAuthorizationHeader(const std::string& value)
{
	authorization_header_ = value;
}

void PeerConnectionClient::OnHeartbeatGetConnect(rtc::AsyncSocket* socket)
{
	auto req = PrepareRequest("GET", "/heartbeat?peer_id=" + std::to_string(my_id_), { { "Host", server_address_.hostname() } });

	int sent = socket->Send(req.c_str(), req.length());
	RTC_DCHECK(sent == req.length());
}

void PeerConnectionClient::OnHeartbeatGetRead(rtc::AsyncSocket* socket)
{
	std::string data;
	size_t content_length = 0;
	if (ReadIntoBuffer(socket, &data, &content_length))
	{
		// empty data can sometimes occur, we wish to ignore it
		if (data.empty())
		{
			return;
		}

		size_t peer_id = 0, eoh = 0;
		int status = GetResponseStatus(data);

		if (status != 200)
		{
			LOG(INFO) << "heartbeat failed (" << status << ")" << (heartbeat_tick_ms_ != kHeartbeatDefault ? ", will retry" : "");
		}
	}

	if (heartbeat_tick_ms_ != kHeartbeatDefault)
	{
		rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, heartbeat_tick_ms_, this, kHeartbeatScheduleId);

		LOG(INFO) << "heartbeat scheduled for " << heartbeat_tick_ms_ << "ms";
	}
}

int PeerConnectionClient::heartbeat_ms() const
{
	return heartbeat_tick_ms_;
}

void PeerConnectionClient::SetHeartbeatMs(const int tickMs)
{
	heartbeat_tick_ms_ = tickMs;
}