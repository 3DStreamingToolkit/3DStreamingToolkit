#include "stdafx.h"

#include "webrtc.h"
#include "webrtcH264.h"
#include "third_party\libyuv\include\libyuv.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace webrtc;

namespace NativeServersUnitTests
{		
	TEST_CLASS(UnitTest1)
	{
	public:
		TEST_METHOD(CanInitializeWithDefaultParameters)
		{
			auto encoder = new H264EncoderImpl(cricket::VideoCodec("H264"));
			VideoCodec codec_settings;
			SetDefaultCodecSettings(&codec_settings);
			Assert::IsTrue(encoder->InitEncode(&codec_settings, kNumCores, kMaxPayloadSize) == WEBRTC_VIDEO_CODEC_OK);

			// Test correct release of encoder
			Assert::IsTrue(encoder->Release() == WEBRTC_VIDEO_CODEC_OK);
			delete encoder;
		}

		TEST_METHOD(HardwareNvencodeEncode) {

			auto h264TestImpl = new H264TestImpl();
			h264TestImpl->SetEncoderHWEnabled(true);
			rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
				h264TestImpl->input_frame_.get()->video_frame_buffer());

			size_t bufferSize = buffer->width() * buffer->height() * 4;
			size_t RowBytes = buffer->width() * 4;
			uint8_t* rgbBuffer = new uint8_t[bufferSize];

			// Convert input frame to RGB
			libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
				buffer->DataU(), buffer->StrideU(),
				buffer->DataV(), buffer->StrideV(),
				rgbBuffer,
				RowBytes,
				buffer->width(), buffer->height());

			// Set RGB frame 
			h264TestImpl->input_frame_.get()->set_frame_buffer(rgbBuffer);

			// Encode frame
			Assert::IsTrue(h264TestImpl->encoder_->Encode(*h264TestImpl->input_frame_, nullptr, nullptr) == WEBRTC_VIDEO_CODEC_OK);
			EncodedImage encoded_frame;

			// Extract encoded_frame from the encoder
			Assert::IsTrue(h264TestImpl->WaitForEncodedFrame(&encoded_frame));

			// Check if we have a complete frame with lengh > 0
			Assert::IsTrue(encoded_frame._completeFrame);
			Assert::IsTrue(encoded_frame._length > 0);

			// Test correct release of encoder
			Assert::IsTrue(h264TestImpl->encoder_->Release() == WEBRTC_VIDEO_CODEC_OK);

			delete[] rgbBuffer;
			rgbBuffer = NULL;
		}
	};
}