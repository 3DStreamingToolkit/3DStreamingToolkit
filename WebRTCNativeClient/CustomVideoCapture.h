#pragma once

//#include "opencv2/videoio.hpp"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocapturerfactory.h"

namespace videocapture {

	class CustomVideoCapturer :
		public cricket::VideoCapturer
	{
	public:
		CustomVideoCapturer(int deviceId);
		virtual ~CustomVideoCapturer();

		// cricket::VideoCapturer implementation.
		virtual cricket::CaptureState Start(const cricket::VideoFormat& capture_format) override;
		virtual void Stop() override;
		virtual bool IsRunning() override;
		virtual bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;
		virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired, cricket::VideoFormat* best_format) override;
		virtual bool IsScreencast() const override;

	private:
		// RTC_DISALLOW_COPY_AND_ASSIGN(CustomVideoCapturer);

		static void* grabCapture(void* arg);

		//to call the SignalFrameCaptured call on the main thread
		void SignalFrameCapturedOnStartThread(const webrtc::VideoFrame* frame);

		//cv::VideoCapture m_VCapture; //opencv capture object
		rtc::Thread*  m_startThread; //video capture thread
	};


	class VideoCapturerFactoryCustom : public cricket::VideoDeviceCapturerFactory
	{
	public:
		VideoCapturerFactoryCustom() {}
		virtual ~VideoCapturerFactoryCustom() {}

		virtual std::unique_ptr<cricket::VideoCapturer> Create(const cricket::Device& device) {

			// XXX: WebRTC uses device name to instantiate the capture, which is always 0.
			return std::unique_ptr<cricket::VideoCapturer>(new CustomVideoCapturer(atoi(device.id.c_str())));
		}
	};

} // namespace videocapture