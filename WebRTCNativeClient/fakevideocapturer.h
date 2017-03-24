/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
#define WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_

#include <string.h>

#include <memory>
#include <vector>
#include <thread>
#include <chrono>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "ppltasks.h"

#include "VideoHelper.h"

using namespace Concurrency;
using namespace Toolkit3DLibrary;

extern VideoHelper*		g_videoHelper;
extern UINT				g_windowWidth;
extern UINT				g_windowHeight;

// Fake video capturer that allows the test to manually pump in frames.
class FakeVideoCapturer : public cricket::VideoCapturer {
public:
	explicit FakeVideoCapturer(bool is_screencast)
		: running_(false),
		initial_timestamp_(rtc::TimeNanos()),
		next_timestamp_(rtc::kNumNanosecsPerMillisec),
		is_screencast_(is_screencast),
		rotation_(webrtc::kVideoRotation_0) {
		// Default supported formats. Use ResetSupportedFormats to over write.
		std::vector<cricket::VideoFormat> formats;
		formats.push_back(cricket::VideoFormat(1280, 720,
			cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
		formats.push_back(cricket::VideoFormat(640, 480,
			cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
		formats.push_back(cricket::VideoFormat(320, 240,
			cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
		formats.push_back(cricket::VideoFormat(160, 120,
			cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
		formats.push_back(cricket::VideoFormat(1280, 720,
			cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420));
		ResetSupportedFormats(formats);
	}
	FakeVideoCapturer() : FakeVideoCapturer(false) {}

	~FakeVideoCapturer() {
		SignalDestroyed(this);
	}

	void ResetSupportedFormats(const std::vector<cricket::VideoFormat>& formats) {
		SetSupportedFormats(formats);
	}
	bool CaptureFrame() {
		if (!GetCaptureFormat()) {
			return false;
		}
		return CaptureCustomFrame(GetCaptureFormat()->width,
			GetCaptureFormat()->height,
			GetCaptureFormat()->interval,
			GetCaptureFormat()->fourcc);
	}

	rtc::scoped_refptr<webrtc::I420Buffer> ReadI420Buffer(
		int width, 
		int height, 
		uint8_t *data) 
	{
		int half_width = (width + 1) / 2;
		rtc::scoped_refptr<webrtc::I420Buffer> buffer(
			// Explicit stride, no padding between rows.
			webrtc::I420Buffer::Create(width, height, width, half_width, half_width));

		size_t size_y = static_cast<size_t>(width) * height;
		size_t size_uv = static_cast<size_t>(half_width) * ((height + 1) / 2);

		memcpy(buffer->MutableDataY(), data, size_y);
		memcpy(buffer->MutableDataU(), data + size_y, size_uv);
		memcpy(buffer->MutableDataV(), data + size_y + size_uv, size_uv);
		return buffer;
	}

	void SendFakeVideoFrame()
	{
		void* pFrameBuffer = nullptr;
		int frameSizeInBytes = 0;
		g_videoHelper->Capture(&pFrameBuffer, &frameSizeInBytes);
		if (frameSizeInBytes == 0)
		{
			return;
		}

		rtc::scoped_refptr<webrtc::I420Buffer> buffer = ReadI420Buffer(
			g_windowWidth,
			g_windowHeight,
			(uint8_t*)pFrameBuffer);

		auto timeStamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		OnFrame(webrtc::VideoFrame(
			buffer, rotation_,
			timeStamp),
			g_windowWidth, g_windowHeight);
	}
	bool CaptureCustomFrame(int width, int height, uint32_t fourcc) {
		// default to 30fps
		return CaptureCustomFrame(width, height, 33333333, fourcc);
	}
	bool CaptureCustomFrame(int width,
		int height,
		int64_t timestamp_interval,
		uint32_t fourcc) {
		if (!running_) {
			return false;
		}
		RTC_CHECK(fourcc == cricket::FOURCC_I420);
		RTC_CHECK(width > 0);
		RTC_CHECK(height > 0);

		int adapted_width;
		int adapted_height;
		int crop_width;
		int crop_height;
		int crop_x;
		int crop_y;

		if (AdaptFrame(width, height, 0, 0, &adapted_width, &adapted_height,
			&crop_width, &crop_height, &crop_x, &crop_y, nullptr)) {
			rtc::scoped_refptr<webrtc::I420Buffer> buffer(
				webrtc::I420Buffer::Create(adapted_width, adapted_height));
			buffer->InitializeData();
		
			auto timeStamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

			OnFrame(webrtc::VideoFrame(
				buffer, rotation_,
				timeStamp),
				width, height);
		}
		next_timestamp_ += timestamp_interval;

		return true;
	}



	sigslot::signal1<FakeVideoCapturer*> SignalDestroyed;

	cricket::CaptureState Start(const cricket::VideoFormat& format) override {
		SetCaptureFormat(&format);

		auto op1 = create_task([this]
		{
			while (running_)
			{
				g_videoHelper->Capture();
				SendFakeVideoFrame();
			}
		});

		running_ = true;
		SetCaptureState(cricket::CS_RUNNING);
		return cricket::CS_RUNNING;
	}

	void Stop() override {
		running_ = false;
		SetCaptureFormat(NULL);
		SetCaptureState(cricket::CS_STOPPED);
	}
	bool IsRunning() override { return running_; }
	bool IsScreencast() const override { return is_screencast_; }
	bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override {
		fourccs->push_back(cricket::FOURCC_I420);
		fourccs->push_back(cricket::FOURCC_MJPG);
		return true;
	}

	void SetRotation(webrtc::VideoRotation rotation) {
		rotation_ = rotation;
	}

	webrtc::VideoRotation GetRotation() { return rotation_; }

private:
	bool running_;
	int64_t initial_timestamp_;
	int64_t next_timestamp_;
	const bool is_screencast_;
	webrtc::VideoRotation rotation_;
};

class VideoCapturerFactoryCustom : public cricket::VideoDeviceCapturerFactory
{
public:
	VideoCapturerFactoryCustom() {}
	virtual ~VideoCapturerFactoryCustom() {}

	virtual std::unique_ptr<cricket::VideoCapturer> Create(const cricket::Device& device) {

		// XXX: WebRTC uses device name to instantiate the capture, which is always 0.
		return std::unique_ptr<cricket::VideoCapturer>(new FakeVideoCapturer(false));
	}
};

#endif  // WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
