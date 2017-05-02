#pragma once

#include <d3d11_4.h>

#include "defs.h"
#include "nvEncodeAPI.h"
#include "nvCPUOPSys.h"

#ifdef USE_WEBRTC_NVENCODE
#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"
#else
#include "NvHWEncoder.h"
#endif

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
		void									GetHeightAndWidth(int* width, int* height);

		// Debug
		ID3D11Device*							m_d3dDevice;
		ID3D11DeviceContext*					m_d3dContext;

	private:

		IDXGISwapChain*							m_swapChain;

		// NvEncoder
		ID3D11Texture2D*						m_stagingFrameBuffer;
		D3D11_TEXTURE2D_DESC					m_stagingFrameBufferDesc;
	};
}
