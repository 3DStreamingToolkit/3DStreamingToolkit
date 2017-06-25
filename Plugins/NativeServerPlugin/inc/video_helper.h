#pragma once

#include <d3d11_4.h>

#include "plugindefs.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvEncodeAPI.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvCPUOPSys.h"

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"

namespace Toolkit3DLibrary
{
    class VideoHelper
    {
	public:
												VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context);
												~VideoHelper();
												
		void									Initialize(IDXGISwapChain* swapChain);
		void									Initialize(ID3D11Texture2D* frameBuffer, DXGI_FORMAT format, int width, int height);
		void									Capture(void** buffer, int* size, int* width, int* height);
		void									Capture(ID3D11Texture2D** texture, int* width, int* height);
		void									GetWidthAndHeight(int* width, int* height);

		ID3D11Device*							m_d3dDevice;
		ID3D11DeviceContext*					m_d3dContext;

	private:
		void									Initialize(DXGI_FORMAT format, int width, int height);
		void									UpdateStagingBuffer(ID3D11Texture2D* frameBuffer);

		IDXGISwapChain*							m_swapChain;
		ID3D11Texture2D*						m_frameBuffer;

		// The staging frame buffer.
		ID3D11Texture2D*						m_stagingFrameBuffer;
		D3D11_TEXTURE2D_DESC					m_stagingFrameBufferDesc;
	};
}
