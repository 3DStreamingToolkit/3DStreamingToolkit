#pragma once

#include <thread>
#include <functional>

#include "webrtc/rtc_base/win32socketinit.h"
#include "webrtc/rtc_base/win32socketserver.h"

/// <summary>
/// Represents the Rtc eventing loop for windows, under the hood
/// </summary>
/// <remarks>
/// Can be used to execute rtc work inside it's own thread
/// </remarks>
/// <example>
/// RtcEventLoop loop([](){ /* do rtc threaded work */ });
/// /* block main thread on rtc threaded work, checking for some completion condition */
/// loop.Kill(); /* or delete loop; */
/// </example>
class RtcEventLoop
{
public:
	/// <summary>
	/// Default ctor
	/// </summary>
	RtcEventLoop(const std::function<void()>& rtcWork);

	/// <summary>
	/// Default dtor
	/// </summary>
	virtual ~RtcEventLoop();

	/// <summary>
	/// Kills the event loop
	/// </summary>
	void Kill();
private:
	bool killed;
	std::function<void()> work;
	std::thread th;
};
