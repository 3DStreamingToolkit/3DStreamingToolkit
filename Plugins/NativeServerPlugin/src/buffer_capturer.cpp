#include "pch.h"

#include <fstream>

#include "buffer_capturer.h"
#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"

namespace StreamingToolkit
{
	BufferCapturer::BufferCapturer() :
		clock_(webrtc::Clock::GetRealTimeClock()),
		running_(false),
		sink_wants_observer_(nullptr)
	{
		use_software_encoder_ = webrtc::H264EncoderImpl::CheckDeviceNVENCCapability() != NVENCSTATUS::NV_ENC_SUCCESS;
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

		RemoveSink(sink);
		sinks_.push_back(sink);

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

		// see https://stackoverflow.com/questions/347441/erasing-elements-from-a-vector
		sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink), sinks_.end());
	}

	void BufferCapturer::SendFrame(webrtc::VideoFrame video_frame)
	{
		// The video capturer hasn't started since there is no active connection.
		if (!running_)
		{
			return;
		}

		if (sinks_.size() > 0)
		{
			for each (auto sink in sinks_)
			{
				sink->OnFrame(video_frame);
			}
		}
		else
		{
			OnFrame(video_frame, video_frame.width(), video_frame.height());
		}
	}
};
