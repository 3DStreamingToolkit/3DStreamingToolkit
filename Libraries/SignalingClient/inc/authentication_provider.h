#pragma once

#include <string>
#include <functional>


struct AuthenticationProviderResult
{
public:
	bool successFlag;
	std::string accessToken;

};

struct AuthenticationProviderObserver
{
	virtual void OnAuthenticationComplete(const AuthenticationProviderResult& result) = 0;

protected:
	virtual ~AuthenticationProviderObserver() {}
};

/// <summary>
/// Base class that represents an authentication provider
/// </summary>
class AuthenticationProvider
{
public:
	AuthenticationProvider() {}

	void RegisterObserver(AuthenticationProviderObserver* callback) { callback_ = callback; }

	virtual bool Authenticate() = 0;

protected:
	virtual ~AuthenticationProvider() {}

	AuthenticationProviderObserver* callback_;
};

