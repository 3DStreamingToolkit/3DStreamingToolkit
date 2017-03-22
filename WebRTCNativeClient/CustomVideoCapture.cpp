#include "CustomVideoCapture.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocapturerfactory.h"
#include <iostream>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

#include <memory>

using std::endl;

namespace videocapture {

#define STREAM "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov"

	pthread_t g_pthread;

	CustomVideoCapturer::CustomVideoCapturer(int deviceId)
	{
		m_VCapture.open(STREAM);
	}

	CustomVideoCapturer::~CustomVideoCapturer()
	{
	}

	cricket::CaptureState CustomVideoCapturer::Start(const cricket::VideoFormat& capture_format)
	{
		std::cout << "Start" << endl;
		if (capture_state() == cricket::CS_RUNNING) {
			std::cout << "Start called when it's already started." << endl;
			return capture_state();
		}

		while (!m_VCapture.isOpened()) {
			std::cout << "Capturer is not open -> will try to reopen" << endl;
			m_VCapture.open(STREAM);
		}
		//get a reference to the current thread so we can send the frames to webrtc
		//on the same thread on which the capture was started
		m_startThread = rtc::Thread::Current();

		//start frame grabbing thread
		pthread_create(&g_pthread, NULL, grabCapture, (void*)this);

		SetCaptureFormat(&capture_format);
		return cricket::CS_RUNNING;
	}

	void CustomVideoCapturer::Stop()
	{
		std::cout << "Stop" << endl;
		if (capture_state() == cricket::CS_STOPPED) {
			std::cout << "Stop called when it's already stopped." << endl;
			return;
		}

		m_startThread = nullptr;

		SetCaptureFormat(NULL);
		SetCaptureState(cricket::CS_STOPPED);
	}

	/*static */void* CustomVideoCapturer::grabCapture(void* arg)
	{
		CustomVideoCapturer *vc = (CustomVideoCapturer*)arg;
		cv::Mat frame;

		if (nullptr == vc) {
			std::cout << "VideoCapturer pointer is null" << std::endl;
			return 0;
		}

		while (vc->m_VCapture.read(frame) && vc->IsRunning()) {
			cv::Mat bgra(frame.rows, frame.cols, CV_8UC4);
			//opencv reads the stream in BGR format by default
			cv::cvtColor(frame, bgra, CV_BGR2BGRA);

			webrtc::VideoFrame vframe;
			if (0 != vframe.CreateEmptyFrame(bgra.cols, bgra.rows, bgra.cols, (bgra.cols + 1) / 2, (bgra.cols + 1) / 2))
			{
				std::cout << "Failed to create empty frame" << std::endl;
			}
			//convert the frame to I420, which is the supported format for webrtc transport
			if (0 != webrtc::ConvertToI420(webrtc::kBGRA, bgra.ptr(), 0, 0, bgra.cols, bgra.rows, 0, webrtc::kVideoRotation_0, &vframe)) {
				std::cout << "Failed to convert frame to i420" << std::endl;
			}
			std::vector<uint8_t> capture_buffer_;
			size_t length = webrtc::CalcBufferSize(webrtc::kI420, vframe.width(), vframe.height());
			capture_buffer_.resize(length);
			webrtc::ExtractBuffer(vframe, length, &capture_buffer_[0]);
			std::shared_ptr<cricket::WebRtcCapturedFrame> webrtc_frame(new cricket::WebRtcCapturedFrame(vframe, &capture_buffer_[0], length));

			//forward the frame to the video capture start thread
			if (vc->m_startThread->IsCurrent()) {
				vc->SignalFrameCaptured(vc, webrtc_frame.get());
			}
			else {
				vc->m_startThread->Invoke<void>(rtc::Bind(&CustomVideoCapturer::SignalFrameCapturedOnStartThread, vc, webrtc_frame.get()));
			}
		}
		return 0;
	}

	void CustomVideoCapturer::SignalFrameCapturedOnStartThread(const cricket::CapturedFrame* frame)
	{
		SignalFrameCaptured(this, frame);
	}

	bool CustomVideoCapturer::IsRunning()
	{
		return capture_state() == cricket::CS_RUNNING;
	}

	bool CustomVideoCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs)
	{
		if (!fourccs)
			return false;
		fourccs->push_back(cricket::FOURCC_I420);
		return true;
	}

	bool CustomVideoCapturer::GetBestCaptureFormat(const cricket::VideoFormat& desired, cricket::VideoFormat* best_format)
	{
		if (!best_format)
			return false;

		// Use the desired format as the best format.
		best_format->width = desired.width;
		best_format->height = desired.height;
		best_format->fourcc = cricket::FOURCC_I420;
		best_format->interval = desired.interval;
		return true;
	}

	bool CustomVideoCapturer::IsScreencast() const
	{
		return false;
	}

} // namespace videocapture