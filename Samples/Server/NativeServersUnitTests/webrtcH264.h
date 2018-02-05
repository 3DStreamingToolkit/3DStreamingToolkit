#pragma once
/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*
*/

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/modules/desktop_capture/win/d3d_device.h"

#include <stdio.h>  /* defines FILENAME_MAX */
#include <direct.h>
#define GetCurrentDir _getcwd

namespace webrtc {

	const int kMaxPayloadSize = 1024;
	const int kNumCores = 1;
	static const int kEncodeTimeoutMs = 100;
	static const int kDecodeTimeoutMs = 25;

	void SetDefaultCodecSettings(VideoCodec* codec_settings) {
		codec_settings->codecType = kVideoCodecH264;
		codec_settings->maxFramerate = 60;
		codec_settings->width = 1280;
		codec_settings->height = 720;
		codec_settings->targetBitrate = 4000;
		codec_settings->maxBitrate = 8000;
		codec_settings->H264()->frameDroppingOn = true;
	}

	class H264TestImpl {
	public:
		H264TestImpl()
			: encode_complete_callback_(this),
			decode_complete_callback_(this),
			encoded_frame_event_(false /* manual reset */,
				false /* initially signaled */),
			decoded_frame_event_(false /* manual reset */,
				false /* initially signaled */)
		{
			defaultCodec = cricket::VideoCodec("H264");
		}

		void SetEncoderHWEnabled(bool value)
		{
			defaultCodec.SetParam(cricket::kH264UseHWNvencode, value);
			SetUp();
		}

		bool WaitForEncodedFrame(EncodedImage* frame) {
			bool ret = encoded_frame_event_.Wait(kEncodeTimeoutMs);
			if (!ret)
				return false;

			// This becomes unsafe if there are multiple threads waiting for frames.
			rtc::CritScope lock(&encoded_frame_section_);
			if (encoded_frame_) {
				*frame = std::move(*encoded_frame_);
				encoded_frame_.reset();
				return true;
			}
			else {
				return false;
			}
		}

		bool WaitForDecodedFrame(std::unique_ptr<VideoFrame>* frame,
			rtc::Optional<uint8_t>* qp) {
			bool ret = decoded_frame_event_.Wait(kDecodeTimeoutMs);
			// EXPECT_TRUE(ret) << "Timed out while waiting for a decoded frame.";
			// This becomes unsafe if there are multiple threads waiting for frames.
			rtc::CritScope lock(&decoded_frame_section_);
			// EXPECT_TRUE(decoded_frame_);
			if (decoded_frame_) {
				frame->reset(new VideoFrame(std::move(*decoded_frame_)));
				decoded_frame_.reset();
				return true;
			}
			else {
				return false;
			}
		}

		std::unique_ptr<VideoFrame> input_frame_;
		std::unique_ptr<VideoEncoder> encoder_;
		std::unique_ptr<VideoDecoder> decoder_;

	protected:
		class FakeEncodeCompleteCallback : public webrtc::EncodedImageCallback {
		public:
			explicit FakeEncodeCompleteCallback(H264TestImpl* test) : test_(test) {}

			Result OnEncodedImage(const EncodedImage& frame,
				const CodecSpecificInfo* codec_specific_info,
				const RTPFragmentationHeader* fragmentation) {
				rtc::CritScope lock(&test_->encoded_frame_section_);
				test_->encoded_frame_ = rtc::Optional<EncodedImage>(frame);
				test_->encoded_frame_event_.Set();
				return Result(Result::OK);
			}

		private:
			H264TestImpl* const test_;
		};

		class FakeDecodeCompleteCallback : public webrtc::DecodedImageCallback {
		public:
			explicit FakeDecodeCompleteCallback(H264TestImpl* test) : test_(test) {}

			int32_t Decoded(VideoFrame& frame) override {

				test_->decoded_frame_ = rtc::Optional<VideoFrame>(frame);
				test_->decoded_frame_event_.Set();

				return WEBRTC_VIDEO_CODEC_OK;
			}
			int32_t Decoded(VideoFrame& frame, int64_t decode_time_ms) override {
				RTC_NOTREACHED();
				return -1;
			}
			void Decoded(VideoFrame& frame,
				rtc::Optional<int32_t> decode_time_ms,
				rtc::Optional<uint8_t> qp) override {
				rtc::CritScope lock(&test_->decoded_frame_section_);
				test_->decoded_frame_ = rtc::Optional<VideoFrame>(frame);
				test_->decoded_qp_ = qp;
				test_->decoded_frame_event_.Set();
			}

		private:
			H264TestImpl* const test_;
		};

		void SetUp() {
			VideoCodec codec_inst_;
			SetDefaultCodecSettings(&codec_inst_);

			char cCurrentPath[FILENAME_MAX];

			if (!GetCurrentDir(cCurrentPath, sizeof(cCurrentPath)))
			{
				return;
			}

			auto path = std::string(cCurrentPath) + "\\paris_qcif.yuv";
			// Using a QCIF image. Processing only one frame.
			FILE* source_file_ =
				fopen(path.c_str(), "rb");
			rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer(
				test::ReadI420Buffer(codec_inst_.width, codec_inst_.height, source_file_));

			input_frame_.reset(new VideoFrame(video_frame_buffer, kVideoRotation_0, 0));
			fclose(source_file_);

			encoder_.reset(H264Encoder::Create(defaultCodec));
			decoder_.reset(H264Decoder::Create());
			encoder_->RegisterEncodeCompleteCallback(&encode_complete_callback_);
			decoder_->RegisterDecodeCompleteCallback(&decode_complete_callback_);
			encoder_->InitEncode(&codec_inst_, 1 /* number of cores */,
				0 /* max payload size (unused) */);
			decoder_->InitDecode(&codec_inst_, 1 /* number of cores */);
		}

	private:
		FakeEncodeCompleteCallback encode_complete_callback_;
		FakeDecodeCompleteCallback decode_complete_callback_;

		cricket::VideoCodec defaultCodec;

		rtc::Event encoded_frame_event_;
		rtc::CriticalSection encoded_frame_section_;
		rtc::Optional<EncodedImage> encoded_frame_ GUARDED_BY(encoded_frame_section_);

		rtc::Event decoded_frame_event_;
		rtc::CriticalSection decoded_frame_section_;
		rtc::Optional<VideoFrame> decoded_frame_ GUARDED_BY(decoded_frame_section_);
		rtc::Optional<uint8_t> decoded_qp_ GUARDED_BY(decoded_frame_section_);
	};
}  // namespace webrtc


