/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include <d3d11_4.h>
#include <wrl\client.h>
#include <wrl\wrappers\corewrappers.h>

#include "macros.h"
#include "buffer_capturer.h"

#ifdef TEST_BUFFER_CAPTURER
FORWARD_DECLARATION(NativeServersUnitTests, BufferCapturerTests)
#endif // TEST_BUFFER_CAPTURER

namespace StreamingToolkit
{
	// Provides DirectX implementation of the BufferCapturer class.
	class DirectXBufferCapturer : public BufferCapturer
	{
	public:
		explicit DirectXBufferCapturer(ID3D11Device* d3d_device);

		virtual ~DirectXBufferCapturer() {}

		void SendFrame(ID3D11Texture2D* frame_buffer, int64_t prediction_time_stamp = -1);

		void SendFrame(ID3D11Texture2D* left_frame_buffer, ID3D11Texture2D* right_frame_buffer, int64_t prediction_time_stamp = -1);

	private:
		void UpdateStagingBuffer(ID3D11Texture2D* frame_buffer);

		void UpdateStagingBuffer(ID3D11Texture2D* left_frame_buffer, ID3D11Texture2D* right_frame_buffer);

		Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_frame_buffer_;
		D3D11_TEXTURE2D_DESC staging_frame_buffer_desc_;

#ifdef TEST_BUFFER_CAPTURER
		FRIEND_CLASS(NativeServersUnitTests, BufferCapturerTests);
#endif // TEST_BUFFER_CAPTURER
	};
}
