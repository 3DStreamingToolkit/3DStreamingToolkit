#include "RtcEventLoop.h"

RtcEventLoop::RtcEventLoop(const std::function<void()>& rtcWork)
{
	killed = false;
	work = rtcWork;
	th = std::thread([&]()
	{
		rtc::EnsureWinsockInit();
		rtc::Win32SocketServer w32_ss;
		rtc::Win32Thread w32_thread(&w32_ss);
		rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

		work();

		while (!w32_thread.IsQuitting() && !killed)
		{
			w32_thread.ProcessMessages(500);
		}
		w32_thread.Quit();
	});
}

RtcEventLoop::~RtcEventLoop()
{
	Kill();
}

void RtcEventLoop::Kill()
{
	killed = true;
	th.join();
}
