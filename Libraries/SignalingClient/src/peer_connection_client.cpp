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
#include "webrtc/base/logging.h"

namespace
{
	// This is our magical hangup signal.
	const char kByeMessage[] = "BYE";

	// Delay between server connection retries, in milliseconds
	const int kReconnectDelay = 2000;
}

PeerConnectionClient::PeerConnectionClient() :
	callback_(NULL),
	state_(NOT_CONNECTED),
	my_id_(-1)
{
}

PeerConnectionClient::~PeerConnectionClient()
{
	// cancel any pending requests
	if (request_async_src_ != nullptr)
	{
		request_async_src_->cancel();
	}
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
	RTC_DCHECK(!callback_);
	callback_ = callback;
}

void PeerConnectionClient::Connect(const std::string& server, int port, 
	const std::string& client_name)
{
	RTC_DCHECK(!server.empty());
	RTC_DCHECK(!client_name.empty());

	if (state_ != NOT_CONNECTED)
	{
		LOG(WARNING) << "The client must not be connected before you can call Connect()";
		callback_->OnServerConnectionFailure();
		return;
	}

	if (server.empty() || port <= 0 || client_name.empty())
	{
		callback_->OnServerConnectionFailure();
		return;
	}

	std::string hostname;
	web::uri_builder uri;

	if (server.substr(0, 7).compare("http://") == 0)
	{
		uri.set_scheme(L"http");
		hostname = server.substr(7);
	}
	else if (server.substr(0, 8).compare("https://") == 0)
	{
		uri.set_scheme(L"https");
		hostname = server.substr(8);
	}
	else
	{
		hostname = server;
	}

	uri.set_host(utility::conversions::to_string_t(hostname));
	uri.set_port(utility::conversions::to_string_t(std::to_string(port)));

	server_address_ = uri.to_uri();
	client_name_ = utility::conversions::to_string_t(client_name);

	auto config = CreateHttpConfig();

	// cancel any pending requests
	if (request_async_src_ != nullptr)
	{
		request_async_src_->cancel();
	}

	http_client_.reset(new web::http::client::http_client(server_address_, config));
	request_async_src_.reset(new concurrency::cancellation_token_source());

	DoConnect();
}

void PeerConnectionClient::DoConnect()
{
	if (state_ == CONNECTED || state_ == SIGNING_IN)
	{
		return;
	}

	web::uri_builder builder(server_address_);
	builder.append_path(L"sign_in");
	builder.append_query(client_name_);

	state_ = SIGNING_IN;

	// schedule the async work to make the request and process it's body (calling the callbacks)
	http_client_->request(web::http::methods::GET, builder.to_string(), request_async_src_->get_token())
		.then(RequestErrorHandler("sign in error", [&](std::exception) { callback_->OnServerConnectionFailure(); }))
		.then([&](web::http::http_response res)
	{
		if (res.status_code() == web::http::status_codes::OK)
		{
			auto pragmaHeader = res.headers()[L"Pragma"];
			my_id_ = static_cast<int>(wcstol(pragmaHeader.c_str(), nullptr, 10));

			// return the async operation of parsing and processing the body
			return res.extract_string()
				.then([&](std::wstring body) { return NotificationBodyParser(body); })
				.then([&](std::vector<std::wstring> lines)
			{
				// parse each line as a peer and fire the callback
				for each (auto line in lines)
				{
					int id = 0;
					std::string name;
					bool connected;

					if (ParseEntry(line, &name, &id, &connected) && id != my_id_)
					{
						peers_[id] = name;
						callback_->OnPeerConnected(id, name);
					}
				}
			}).then([&]()
			{
				// starting waiting after signing in
				ConfigureHangingGet();

				state_ = CONNECTED;
				callback_->OnSignedIn();
			});
		}
		else
		{
            state_ = NOT_CONNECTED;
			callback_->OnServerConnectionFailure();

			// just return a task to keep the return type constant here
			return concurrency::create_task([] {});
		}
	});
}

void PeerConnectionClient::ConfigureHangingGet()
{
	if (hanging_http_client_ == nullptr)
	{
		auto config = CreateHttpConfig();

		hanging_http_client_.reset(new web::http::client::http_client(server_address_, config));
	}

	web::uri_builder builder(server_address_);
	builder.append_path(L"wait");
	builder.append_query(L"peer_id", std::to_wstring(my_id_));

	// schedule the async work to make the request and process it's body (calling the callbacks)
	hanging_http_client_->request(web::http::methods::GET, builder.to_string(), request_async_src_->get_token())
		.then(RequestErrorHandler("waiting error"))
		.then([&](web::http::http_response res)
	{
		if (res.status_code() != web::http::status_codes::OK)
		{
			// just return a task to keep the chain going
			// but don't proceed with the logic in this continuation block
			return concurrency::create_task([] {});
		}

		auto pragmaHeader = res.headers()[L"Pragma"];
		auto peer_id = static_cast<int>(wcstol(pragmaHeader.c_str(), nullptr, 10));

		// parse peers list, or pass message upward
		return res.extract_string()
			.then([&, peer_id](std::wstring body)
		{
			if (my_id_ == peer_id)
			{
				auto lines = NotificationBodyParser(body);

				for each (auto line in lines)
				{
					int id = 0;
					std::string name;
					bool connected;

					if (ParseEntry(line, &name, &id, &connected))
					{
						if (connected)
						{
							peers_[id] = name;
							callback_->OnPeerConnected(id, name);
						}
						else
						{
							peers_.erase(id);
							callback_->OnPeerDisconnected(id);
						}
					}
				}
			}
			else
			{
				OnMessageFromPeer(peer_id, body);
			}
		});
	})
		.then([&]()
	{
		// start another hanging get
		if (state_ == CONNECTED)
		{
			ConfigureHangingGet();
		}
	});
}

bool PeerConnectionClient::SendToPeer(int peer_id, const std::string& message)
{
	if (state_ != CONNECTED)
	{
		return false;
	}

	RTC_DCHECK(is_connected());
	if (!is_connected() || peer_id == -1)
	{
		return false;
	}

	web::uri_builder builder(server_address_);
	builder.append_path(L"message");

	// the naming here isn't a bug, it's just confusing
	builder.append_query(L"peer_id", std::to_wstring(my_id_));
	builder.append_query(L"to", std::to_wstring(peer_id));

	// make the request
	http_client_->request(web::http::methods::POST, builder.to_string(), utility::conversions::to_string_t(message), request_async_src_->get_token())
		.then(RequestErrorHandler("send to peer error"))
		.then([&](web::http::http_response res)
	{
		callback_->OnMessageSent(res.status_code());
	});

	return true;
}

bool PeerConnectionClient::SendHangUp(int peer_id)
{
	return SendToPeer(peer_id, kByeMessage);
}

bool PeerConnectionClient::IsSendingMessage()
{
	// since our http implementation internally buffers messages
	// we can always "send" more, so we mark ourselves as never stuck sending messages
	return false;
}

bool PeerConnectionClient::SignOut()
{
	if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
	{
		return true;
	}

	state_ = SIGNING_OUT;

	if (my_id_ != -1)
	{
		web::uri_builder builder(server_address_);
		builder.append_path(L"sign_out");
		builder.append_query(L"peer_id", std::to_wstring(my_id_));

		state_ = SIGNING_OUT_WAITING;

		// make the request
		http_client_->request(web::http::methods::GET, builder.to_string(), request_async_src_->get_token())
			.then(RequestErrorHandler("sign out error"))
			.then([&](web::http::http_response res)
		{
			if (res.status_code() == web::http::status_codes::OK)
			{
				Close();
				callback_->OnDisconnected();
			}
		});

		return true;
	}
	else
	{
		// Can occur if the app is closed before we finish connecting.
		return true;
	}

	return true;
}

void PeerConnectionClient::Close()
{
	peers_.clear();

	if (request_async_src_ != nullptr)
	{
		request_async_src_->cancel();
	}

	my_id_ = -1;
	state_ = NOT_CONNECTED;
}

void PeerConnectionClient::OnMessageFromPeer(int peer_id, const std::wstring& message)
{
	auto convertedMessage = utility::conversions::to_utf8string(message);

	if (convertedMessage.length() == (sizeof(kByeMessage) - 1) &&
		convertedMessage.compare(kByeMessage) == 0)
	{
		callback_->OnPeerDisconnected(peer_id);
	}
	else
	{
		callback_->OnMessageFromPeer(peer_id, convertedMessage);
	}
}

web::http::client::http_client_config PeerConnectionClient::CreateHttpConfig()
{
	web::http::client::http_client_config config;
	config.set_timeout(utility::seconds(60 * 5));
	config.set_validate_certificates(true);

	if (proxy_address_.length() > 0)
	{
		config.set_proxy(web::web_proxy(utility::conversions::to_string_t(proxy_address_)));
	}

	return config;
}

bool PeerConnectionClient::ParseEntry(const std::wstring& entry, std::string* name, 
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
		// assumption here is that the id of an entry is representable as an int
		*id = wcstol(&entry[separator + 1], nullptr, 10);

		name->assign(utility::conversions::to_utf8string(entry.substr(0, separator)));
		separator = entry.find(',', separator + 1);

		if (separator != std::wstring::npos) 
		{
			*connected = wcstol(&entry[separator + 1], nullptr, 10) ? true : false;
		}
	}

	return !name->empty();
}

std::vector<std::wstring> PeerConnectionClient::NotificationBodyParser(std::wstring body)
{
	// split the body by \n into a vector
	std::vector<std::wstring> result;
	auto resultInserter = std::back_inserter(result);
	std::wstringstream ss;
	ss.str(body);
	std::wstring item;
	while (std::getline(ss, item, wchar_t('\n')))
	{
		(resultInserter++) = item;
	}
	return result;
}

std::function<web::http::http_response(concurrency::task<web::http::http_response>)> PeerConnectionClient::RequestErrorHandler(std::string errorContext,
	std::function<void(std::exception)> callback)
{
	return [errorContext, callback](concurrency::task<web::http::http_response> previousTask)
	{
		try
		{
			return previousTask.get();
		}
		catch (std::exception& ex)
		{
			LOG(WARNING) << "Request failed " << ex.what() << "(" << errorContext << ")";

			if (callback != nullptr)
			{
				callback(ex);
			}
		}

		// an empty response with a status code that indicates the error
		return web::http::http_response(web::http::status_codes::PreconditionFailed);
	};
}

void PeerConnectionClient::OnMessage(rtc::Message* msg) 
{
	// ignore msg; there is currently only one supported message ("retry")
	DoConnect();
}

void PeerConnectionClient::set_proxy(const std::string& proxy)
{
	proxy_address_ = proxy;
}

const std::string& PeerConnectionClient::proxy() const
{
	return proxy_address_;
}