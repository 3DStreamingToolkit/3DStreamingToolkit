#pragma once

#include <d3d11_4.h>

#include "defs.h"
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
												
		NVENCSTATUS								Initialize(IDXGISwapChain* swapChain);
		NVENCSTATUS                             Deinitialize(); 
		void									Capture(void** buffer, int* size, int* width, int* height);
		ID3D11Texture2D*						Capture2DTexture(int* width, int* height);
		void									GetWidthAndHeight(int* width, int* height);

		ID3D11Device*							m_d3dDevice;
		ID3D11DeviceContext*					m_d3dContext;

	private:
		IDXGISwapChain*							m_swapChain;

		// NvEncoder
		ID3D11Texture2D*						m_stagingFrameBuffer;
		D3D11_TEXTURE2D_DESC					m_stagingFrameBufferDesc;
	};
}
