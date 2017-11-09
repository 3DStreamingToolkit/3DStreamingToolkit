#include "pch.h"
#include "custom_video_capturer.h"
#include <fstream>

namespace StreamingToolkit
{
	class CustomVideoCapturer::InsertFrameTask : public rtc::QueuedTask 
	{
	public:
		// Repeats in |repeat_interval_ms|. One-time if |repeat_interval_ms| == 0.
		InsertFrameTask(
			CustomVideoCapturer* frame_generator_capturer,
			uint32_t repeat_interval_ms)
			: frame_generator_capturer_(frame_generator_capturer),
			repeat_interval_ms_(repeat_interval_ms),
			intended_run_time_ms_(-1) {}

	private:
		bool Run() override 
		{
			bool task_completed = true;
			if (repeat_interval_ms_ > 0) 
			{
				// This is not a one-off frame. Check if the frame interval for this
				// task queue is the same same as the current configured frame rate.
				uint32_t current_interval_ms =
					1000 / frame_generator_capturer_->GetCurrentConfiguredFramerate();

				if (repeat_interval_ms_ != current_interval_ms) 
				{
					// Frame rate has changed since task was started, create a new instance.
					rtc::TaskQueue::Current()->PostDelayedTask(
						std::unique_ptr<rtc::QueuedTask>(new InsertFrameTask(
							frame_generator_capturer_, current_interval_ms)),
						current_interval_ms);
				}
				else 
				{
					// Schedule the next frame capture event to happen at approximately the
					// correct absolute time point.
					int64_t delay_ms;
					int64_t time_now_ms = rtc::TimeMillis();
					if (intended_run_time_ms_ > 0) 
					{
						delay_ms = time_now_ms - intended_run_time_ms_;
					}
					else 
					{
						delay_ms = 0;
						intended_run_time_ms_ = time_now_ms;
					}

					intended_run_time_ms_ += repeat_interval_ms_;
					if (delay_ms < repeat_interval_ms_) 
					{
						rtc::TaskQueue::Current()->PostDelayedTask(
							std::unique_ptr<rtc::QueuedTask>(this),
							repeat_interval_ms_ - delay_ms);
					}
					else 
					{
						rtc::TaskQueue::Current()->PostDelayedTask(
							std::unique_ptr<rtc::QueuedTask>(this), 0);
					}

					// Repost of this instance, make sure it is not deleted.
					task_completed = false;
				}
			}

			frame_generator_capturer_->InsertFrame();

			// Task should be deleted only if it's not repeating.
			return task_completed;
		}

		CustomVideoCapturer* const frame_generator_capturer_;
		const uint32_t repeat_interval_ms_;
		int64_t intended_run_time_ms_;
	};

	CustomVideoCapturer::CustomVideoCapturer(webrtc::Clock* clock,
		StreamingToolkit::BufferRenderer* buffer_renderer,
		int target_fps) : 
		clock_(clock),
		sending_(false),
		sink_(nullptr),
		use_software_encoder_(false),
		sink_wants_observer_(nullptr),
		target_fps_(target_fps),
		buffer_renderer_(buffer_renderer),
		staging_video_texture_(nullptr),
		first_frame_capture_time_(-1),
		task_queue_("FrameGenCapQ",
			rtc::TaskQueue::Priority::HIGH)
	{
		SetCaptureFormat(NULL);
		set_enable_video_adapter(false);

		// Default supported formats. Use ResetSupportedFormats to over write.
		std::vector<cricket::VideoFormat> formats;
		formats.push_back(cricket::VideoFormat(640, 480, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_H264));
		formats.push_back(cricket::VideoFormat(1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_H264));
		formats.push_back(cricket::VideoFormat(640, 480, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_H264));
		formats.push_back(cricket::VideoFormat(1280, 720, cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_H264));
		SetSupportedFormats(formats);
	}

	int CustomVideoCapturer::GetCurrentConfiguredFramerate() 
	{
		rtc::CritScope cs(&lock_);
		if (wanted_fps_ && *wanted_fps_ < target_fps_)
			return *wanted_fps_;

		return target_fps_;
	}

	bool CustomVideoCapturer::Init() 
	{
		int framerate_fps = GetCurrentConfiguredFramerate();
		task_queue_.PostDelayedTask(
			std::unique_ptr<rtc::QueuedTask>(new InsertFrameTask(this, 1000 / framerate_fps)),
			1000 / framerate_fps);

		return true;
	}

	cricket::CaptureState CustomVideoCapturer::Start(const cricket::VideoFormat& format) 
	{
		SetCaptureFormat(&format);

		Json::Reader reader;
		Json::Value root = NULL;

		auto encoder_config_path = ExePath("nvEncConfig.json");
		std::ifstream file(encoder_config_path);
		if (file.good())
		{
			file >> root;
			reader.parse(file, root, true);

			if (root.isMember("serverFrameCaptureFPS")) 
			{
				target_fps_ = root.get("serverFrameCaptureFPS", 60).asInt();
			}

			if (root.isMember("useSoftwareEncoding")) 
			{
				use_software_encoder_ = root.get("useSoftwareEncoding", false).asBool();
			}
		}
		else // default to 60 fps and hardward encoder in case of missing config file.
		{
			target_fps_ = 60;
			use_software_encoder_ = false;
		}

		Init();
		running_ = true;
		sending_ = true;
		SetCaptureState(cricket::CS_RUNNING);
		return cricket::CS_RUNNING;
	}

	void CustomVideoCapturer::InsertFrame() 
	{
		rtc::CritScope cs(&lock_);
		if (sending_) 
		{
			if (!buffer_renderer_->IsLocked())
			{
				return;
			}

			// Updates buffer renderer.
			buffer_renderer_->Render();

			rtc::scoped_refptr<webrtc::I420Buffer> buffer;
			ID3D11Texture2D* captured_texture = nullptr;
			void* frame_buffer = nullptr;
			int width = 0;
			int height = 0;

			// Captures buffer renderer.
			if (use_software_encoder_)
			{
				int frame_size_in_bytes = 0;
				buffer_renderer_->Capture(&frame_buffer, &frame_size_in_bytes, &width, &height);
				if (frame_size_in_bytes == 0)
				{
					return;
				}
			}
			else
			{
				captured_texture = buffer_renderer_->Capture();
				if (!captured_texture)
				{
					return;
				}
			}

			// Creates video frame buffer.
			buffer_renderer_->GetDimension(&width, &height);
			buffer = webrtc::I420Buffer::Create(width, height);

			// For software encoder, converting to supported video format.
			if (use_software_encoder_)
			{
				libyuv::ABGRToI420(
					(uint8_t*)frame_buffer,
					width * 4,
					buffer.get()->MutableDataY(),
					buffer.get()->StrideY(),
					buffer.get()->MutableDataU(),
					buffer.get()->StrideU(),
					buffer.get()->MutableDataV(),
					buffer.get()->StrideV(),
					width,
					height);
			}
			else
			{
				if (!staging_video_texture_)
				{
					D3D11_TEXTURE2D_DESC captured_texture_desc;
					captured_texture->GetDesc(&captured_texture_desc);

					D3D11_TEXTURE2D_DESC texture_desc = { 0 };
					texture_desc.ArraySize = captured_texture_desc.ArraySize;
					texture_desc.Usage = D3D11_USAGE_DEFAULT;
					texture_desc.Format = captured_texture_desc.Format;
					texture_desc.Width = captured_texture_desc.Width;
					texture_desc.Height = captured_texture_desc.Height;
					texture_desc.MipLevels = captured_texture_desc.MipLevels;
					texture_desc.SampleDesc.Count = captured_texture_desc.SampleDesc.Count;
					buffer_renderer_->GetD3DDevice()->CreateTexture2D(
						&texture_desc, nullptr, &staging_video_texture_);
				}

				buffer_renderer_->GetD3DDeviceContext()->CopyResource(
					staging_video_texture_, captured_texture);
			}

			// Updates time stamp.
			auto time_stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

			// Creates video frame.
			auto frame = webrtc::VideoFrame(buffer, fake_rotation_, time_stamp);

			// For hardware encoder, setting the video frame texture to send.
			if (!use_software_encoder_)
			{
				staging_video_texture_->AddRef();
				frame.set_staging_frame_buffer(staging_video_texture_);
			}

			frame.set_prediction_timestamp(buffer_renderer_->GetPredictionTimestamp());
			frame.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
			frame.set_rotation(fake_rotation_);
			if (first_frame_capture_time_ == -1) 
			{
				first_frame_capture_time_ = frame.ntp_time_ms();
			}

			if (sink_) 
			{
				sink_->OnFrame(frame);
			}
			else
			{
				OnFrame(frame, width, height);
			}

			buffer_renderer_->Unlock();
		}
	}

	void CustomVideoCapturer::Stop() 
	{
		rtc::CritScope cs(&lock_);
		sending_ = false;
	}

	void CustomVideoCapturer::SetSinkWantsObserver(SinkWantsObserver* observer) 
	{
		rtc::CritScope cs(&lock_);
		RTC_DCHECK(!sink_wants_observer_);
		sink_wants_observer_ = observer;
	}

	void CustomVideoCapturer::AddOrUpdateSink(
		rtc::VideoSinkInterface<VideoFrame>* sink,
		const rtc::VideoSinkWants& wants) 
	{
		rtc::CritScope cs(&lock_);
		//RTC_CHECK(!sink_ || sink_ == sink);
		sink_ = sink;
		if (sink_wants_observer_)
			sink_wants_observer_->OnSinkWantsChanged(sink, wants);
	}

	bool CustomVideoCapturer::IsRunning() { return running_; }
	bool CustomVideoCapturer::IsScreencast() const { return false; }

	bool CustomVideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) 
	{
		fourccs->push_back(cricket::FOURCC_H264);
		return true;
	}

	void CustomVideoCapturer::SetFakeRotation(VideoRotation rotation) 
	{
		rtc::CritScope cs(&lock_);
		fake_rotation_ = rotation;
	}

	void CustomVideoCapturer::ChangeResolution(size_t width, size_t height) 
	{
		//TODO - change nvencode settings
	}

	void CustomVideoCapturer::RemoveSink(
		rtc::VideoSinkInterface<VideoFrame>* sink) 
	{
		rtc::CritScope cs(&lock_);
		//RTC_CHECK(sink_ == sink);
		sink_ = nullptr;
	}

	void CustomVideoCapturer::ForceFrame() 
	{
		// One-time non-repeating task,
		// therefore repeat_interval_ms is 0 in InsertFrameTask()
		task_queue_.PostTask(
			std::unique_ptr<rtc::QueuedTask>(new InsertFrameTask(this, 0)));
	}
};
