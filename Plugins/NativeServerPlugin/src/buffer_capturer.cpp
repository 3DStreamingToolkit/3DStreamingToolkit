#include "pch.h"

#include <fstream>

#include "buffer_capturer.h"

namespace StreamingToolkit
{
	BufferCapturer::BufferCapturer() :
		clock_(webrtc::Clock::GetRealTimeClock()),
		running_(false),
		sink_(nullptr),
		use_software_encoder_(false),
		sink_wants_observer_(nullptr)
	{
		set_enable_video_adapter(false);
		SetCaptureFormat(NULL);
	}

	cricket::CaptureState BufferCapturer::Start(const cricket::VideoFormat& format)
	{
		SetCaptureFormat(&format);
		running_ = true;
		SetCaptureState(cricket::CS_RUNNING);
		return cricket::CS_RUNNING;
	}

	void BufferCapturer::Stop()
	{
		rtc::CritScope cs(&lock_);
		running_ = false;
	}

	void BufferCapturer::SetSinkWantsObserver(SinkWantsObserver* observer)
	{
		rtc::CritScope cs(&lock_);
		RTC_DCHECK(!sink_wants_observer_);
		sink_wants_observer_ = observer;
	}

	void BufferCapturer::AddOrUpdateSink(
		rtc::VideoSinkInterface<VideoFrame>* sink,
		const rtc::VideoSinkWants& wants) 
	{
		rtc::CritScope cs(&lock_);
		sink_ = sink;
		if (sink_wants_observer_)
		{
			sink_wants_observer_->OnSinkWantsChanged(sink, wants);
		}
	}

	bool BufferCapturer::IsRunning() 
	{
		return running_;
	}

	bool BufferCapturer::IsScreencast() const 
	{
		return false;
	}

	bool BufferCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs)
	{
		fourccs->push_back(cricket::FOURCC_H264);
		return true;
	}

	void BufferCapturer::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) 
	{
		rtc::CritScope cs(&lock_);
		sink_ = nullptr;
	}

	void BufferCapturer::EnableSoftwareEncoder(bool use_software_encoder)
	{
		use_software_encoder_ = use_software_encoder;
	}

	void BufferCapturer::SendFrame(webrtc::VideoFrame video_frame)
	{
		if (!running_)
		{
			return;
		}

		if (sink_)
		{
			sink_->OnFrame(video_frame);
		}
		else
		{
			OnFrame(video_frame, video_frame.width(), video_frame.height());
		}
	}
};
