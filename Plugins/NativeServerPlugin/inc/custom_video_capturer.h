/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <string.h>

#include <memory>
#include <vector>
#include <thread>
#include <chrono>

#include "webrtc/base/asyncinvoker.h"

#include "webrtc/typedefs.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocapturerfactory.h"
#include "webrtc/media/base/videocommon.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "ppltasks.h"

#include "buffer_renderer.h"
#include "libyuv/convert.h"

using namespace Concurrency;
using namespace webrtc;

namespace StreamingToolkit
{
	class SinkWantsObserver 
	{
	public:
		// OnSinkWantsChanged is called when FrameGeneratorCapturer::AddOrUpdateSink
		// is called.
		virtual void OnSinkWantsChanged(rtc::VideoSinkInterface<VideoFrame>* sink,
			const rtc::VideoSinkWants& wants) = 0;

	protected:
		virtual ~SinkWantsObserver() {}
	};

	// Fake video capturer that allows the test to manually pump in frames.
	class CustomVideoCapturer : public cricket::VideoCapturer
	{
	public:
		explicit CustomVideoCapturer(
			webrtc::Clock* clock,
			StreamingToolkit::BufferRenderer* buffer_renderer,
			int target_fps);

		CustomVideoCapturer();

		~CustomVideoCapturer()
		{
			SignalDestroyed(this);
		}

		// Returns the size in bytes of the input RGBA frames.
		int InputFrameSize(int width, int height) const
		{
			return width*height * 4;
		}

		// Returns the size of the Y plane in bytes.
		int YPlaneSize(int width, int height) const
		{
			return width*height;
		}

		// Returns the size of the U plane in bytes.
		int UPlaneSize(int width, int height) const
		{
			return ((width + 1) / 2)*((height) / 2);
		}

		// Returns the size of the V plane in bytes.
		int VPlaneSize(int width, int height) const
		{
			return ((width + 1) / 2)*((height) / 2);
		}

		// Returns the number of bytes per row in the RGBA frame.
		int SrcStrideFrame(int width) const
		{
			return width * 4;
		}

		// Returns the number of bytes in the Y plane.
		int DstStrideY(int width) const
		{
			return width;
		}

		// Returns the number of bytes in the U plane.
		int DstStrideU(int width) const
		{
			return (width + 1) / 2;
		}

		// Returns the number of bytes in the V plane.
		int DstStrideV(int width) const
		{
			return (width + 1) / 2;
		}

		cricket::CaptureState Start(const cricket::VideoFormat& capture_format) override;
		void Stop() override;
		void ChangeResolution(size_t width, size_t height);

		void SetSinkWantsObserver(SinkWantsObserver* observer);
		bool IsRunning() override;
		bool IsScreencast() const override;
		bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;

		void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
			const rtc::VideoSinkWants& wants) override;

		void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

		void ForceFrame();
		void SetFakeRotation(VideoRotation rotation);

		int64_t first_frame_capture_time() const { return first_frame_capture_time_; }

		sigslot::signal1<CustomVideoCapturer*> SignalDestroyed;
		bool Init();

	private:
		class InsertFrameTask;

		void InsertFrame();
		int GetCurrentConfiguredFramerate();
		Clock* const clock_;
		bool use_software_encoder_;
		int target_fps_ GUARDED_BY(&lock_);
		rtc::Optional<int> wanted_fps_ GUARDED_BY(&lock_);
		VideoRotation fake_rotation_ = kVideoRotation_0;
		bool sending_;
		bool running_;
		rtc::VideoSinkInterface<VideoFrame>* sink_ GUARDED_BY(&lock_);
		SinkWantsObserver* sink_wants_observer_ GUARDED_BY(&lock_);
		rtc::CriticalSection lock_;
		StreamingToolkit::BufferRenderer* buffer_renderer_;
		ID3D11Texture2D* staging_video_texture_;

		int64_t first_frame_capture_time_;

		// Must be the last field, so it will be deconstructed first as tasks
		// in the TaskQueue access other fields of the instance of this class.
		rtc::TaskQueue task_queue_;
	};

	class VideoCapturerFactoryCustom : public cricket::VideoDeviceCapturerFactory
	{
	public:
		VideoCapturerFactoryCustom() {}

		virtual ~VideoCapturerFactoryCustom() {}

		virtual std::unique_ptr<cricket::VideoCapturer> Create(const cricket::Device& device)
		{
			// XXX: WebRTC uses device name to instantiate the capture, which is always 0.
			return std::unique_ptr<cricket::VideoCapturer>(
				new CustomVideoCapturer(nullptr, nullptr, 0));
		}

		virtual std::unique_ptr<cricket::VideoCapturer> Create(
			webrtc::Clock* clock,
			const cricket::Device& device,
			StreamingToolkit::BufferRenderer* buffer_renderer)
		{
			// XXX: WebRTC uses device name to instantiate the capture, which is always 0.
			return std::unique_ptr<cricket::VideoCapturer>(
				new CustomVideoCapturer(clock, buffer_renderer, 0));
		}
	};
}
