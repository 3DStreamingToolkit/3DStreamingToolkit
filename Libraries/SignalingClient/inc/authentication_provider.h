#pragma once

#include <string>
#include <functional>

struct AuthenticationProviderObserver
{
	virtual void OnAuthenticationComplete(const AuthenticationProvider::Result& result) = 0;

protected:
	virtual ~AuthenticationProviderObserver() {}
};

/// <summary>
/// Base class that represents an authentication provider
/// </summary>
class AuthenticationProvider
{
public:
	struct Result
	{
	public:
		bool successFlag;
		std::string accessToken;
	};

	AuthenticationProvider() {}

	void RegisterObserver(AuthenticationProviderObserver* callback) { callback_ = callback; }

	virtual bool Authenticate() = 0;

protected:
	virtual ~AuthenticationProvider() {}

	AuthenticationProviderObserver* callback_;
};

