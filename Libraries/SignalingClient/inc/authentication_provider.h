#pragma once

#include <string>
#include <functional>

/// <summary>
/// Represents a result from an authentication provider authenticate operation
/// </summary>
struct AuthenticationProviderResult
{
public:
	bool successFlag;
	std::string accessToken;

};

/// <summary>
/// Base class that represents an authentication provider
/// </summary>
class AuthenticationProvider
{
public:
	AuthenticationProvider() {}

	sigslot::signal1<const AuthenticationProviderResult&> SignalAuthenticationComplete;

	struct AuthenticationCompleteCallback : public sigslot::has_slots<>
	{
		AuthenticationCompleteCallback(const std::function<void(const AuthenticationProviderResult&)>& handler) : handler_(handler)
		{
		}

		void Handle(const AuthenticationProviderResult& data)
		{
			handler_(data);
		}
	private:
		std::function<void(const AuthenticationProviderResult&)> handler_;
	};

	virtual bool Authenticate() = 0;

protected:
	virtual ~AuthenticationProvider() {}
};
