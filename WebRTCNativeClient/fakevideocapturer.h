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
//#include "libyuv/compare.h"  // NOLINT
//#include "libyuv/convert.h"  // NOLINT
#include "ppltasks.h"

#include "VideoHelper.h"
#include "directx\DeviceResources.h"
#include "libyuv/convert.h"

using namespace Concurrency;
using namespace Toolkit3DLibrary;

extern VideoHelper*		g_videoHelper;

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
		formats.push_back(cricket::VideoFormat(1420, 700,
			cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_H264));
		ResetSupportedFormats(formats);
	}
	FakeVideoCapturer() : FakeVideoCapturer(false) {}

	~FakeVideoCapturer() {
		SignalDestroyed(this);
	}

	// Returns the size in bytes of the input RGBA frames.
	int InputFrameSize(int width, int height) const {
		return width*height * 4;
	}

	// Returns the size of the Y plane in bytes.
	int YPlaneSize(int width, int height) const {
		return width*height;
	}

	// Returns the size of the U plane in bytes.
	int UPlaneSize(int width, int height) const {
		return ((width + 1) / 2)*((height) / 2);
	}

	// Returns the size of the V plane in bytes.
	int VPlaneSize(int width, int height) const {
		return ((width + 1) / 2)*((height) / 2);
	}

	// Returns the number of bytes per row in the RGBA frame.
	int SrcStrideFrame(int width) const {
		return width * 4;
	}

	// Returns the number of bytes in the Y plane.
	int DstStrideY(int width) const {
		return width;
	}

	// Returns the number of bytes in the U plane.
	int DstStrideU(int width) const {
		return (width + 1) / 2;
	}

	// Returns the number of bytes in the V plane.
	int DstStrideV(int width) const {
		return (width + 1) / 2;
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

	rtc::scoped_refptr<webrtc::I420Buffer> CreateGradient(int width, int height) {
		rtc::scoped_refptr<webrtc::I420Buffer> buffer(
			webrtc::I420Buffer::Create(width, height));
		// Initialize with gradient, Y = 128(x/w + y/h), U = 256 x/w, V = 256 y/h
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				buffer->MutableDataY()[x + y * width] =
					128 * (x * height + y * width) / (width * height);
			}
		}
		int chroma_width = (width + 1) / 2;
		int chroma_height = (height + 1) / 2;
		for (int x = 0; x < chroma_width; x++) {
			for (int y = 0; y < chroma_height; y++) {
				buffer->MutableDataU()[x + y * chroma_width] =
					255 * x / (chroma_width - 1);
				buffer->MutableDataV()[x + y * chroma_width] =
					255 * y / (chroma_height - 1);
			}
		}
		return buffer;
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

		rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(
			FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

		libyuv::ARGBToI420(
			(uint8_t*)pFrameBuffer,
			FRAME_BUFFER_WIDTH * 4,
			buffer.get()->MutableDataY(),
			buffer.get()->StrideY(),
			buffer.get()->MutableDataU(),
			buffer.get()->StrideU(),
			buffer.get()->MutableDataV(),
			buffer.get()->StrideV(),
			FRAME_BUFFER_WIDTH,
			FRAME_BUFFER_HEIGHT);

		auto timeStamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		OnFrame(webrtc::VideoFrame(
			buffer, rotation_,
			timeStamp),
			FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
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
		fourccs->push_back(cricket::FOURCC_H264);
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
