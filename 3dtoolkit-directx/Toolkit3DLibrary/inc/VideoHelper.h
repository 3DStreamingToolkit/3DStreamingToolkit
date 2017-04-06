#pragma once

#include <d3d11.h>

#include "defs.h"
#include "nvEncodeAPI.h"
#include "nvCPUOPSys.h"
#include "NvHWEncoder.h"

namespace Toolkit3DLibrary
{
	template<class T>
	class CNvQueue 
	{
		T** m_pBuffer;
		unsigned int m_uSize;
		unsigned int m_uPendingCount;
		unsigned int m_uAvailableIdx;
		unsigned int m_uPendingndex;

	public:
		CNvQueue() : 
			m_uSize(0),
			m_uPendingCount(0),
			m_uAvailableIdx(0),
			m_uPendingndex(0),
			m_pBuffer(NULL)
		{
		}

		~CNvQueue()
		{
			delete[] m_pBuffer;
		}

		bool Initialize(T* pItems, unsigned int uSize)
		{
			m_uSize = uSize;
			m_uPendingCount = 0;
			m_uAvailableIdx = 0;
			m_uPendingndex = 0;
			m_pBuffer = new T*[m_uSize];
			for (unsigned int i = 0; i < m_uSize; i++)
			{
				m_pBuffer[i] = &pItems[i];
			}

			return true;
		}

		T* GetAvailable()
		{
			T* pItem = NULL;
			if (m_uPendingCount == m_uSize)
			{
				return NULL;
			}

			pItem = m_pBuffer[m_uAvailableIdx];
			m_uAvailableIdx = (m_uAvailableIdx + 1) % m_uSize;
			m_uPendingCount += 1;
			return pItem;
		}

		T* GetPending()
		{
			if (m_uPendingCount == 0)
			{
				return NULL;
			}

			T* pItem = m_pBuffer[m_uPendingndex];
			m_uPendingndex = (m_uPendingndex + 1) % m_uSize;
			m_uPendingCount -= 1;
			return pItem;
		}
	};

	typedef struct _EncodeFrameConfig
	{
		ID3D11Texture2D* pRGBTexture;
		uint32_t width;
		uint32_t height;
	} EncodeFrameConfig;

    class VideoHelper
    {
	public:
												VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context);
												~VideoHelper();
												
		NVENCSTATUS								Initialize(IDXGISwapChain* swapChain, char* outputFile = nullptr);
		NVENCSTATUS								Initialize(IDXGISwapChain* swapChain, EncodeConfig nvEncodeConfig);
		NVENCSTATUS                             Deinitialize();
		NVENCSTATUS								SetEncodeProfile(int profileIndex);
		void									GetDefaultEncodeConfig(EncodeConfig &nvEncodeConfig);
		void									Capture();
		void									Capture(void** buffer, int* size, int* width, int* height);
		void									CaptureEncodedFrame(void** buffer, int* size);

		// Debug
		void									PrintConfig(EncodeConfig encodeConfig);

	private:
		ID3D11Device*							m_d3dDevice;
		ID3D11DeviceContext*					m_d3dContext;
		IDXGISwapChain*							m_swapChain;

		// NvEncoder
		CNvHWEncoder*                           m_pNvHWEncoder;
		uint32_t                                m_uEncodeBufferCount;
		EncodeOutputBuffer						m_stEOSOutputBfr;
		EncodeBuffer							m_stEncodeBuffer[MAX_ENCODE_QUEUE];
		CNvQueue<EncodeBuffer>                  m_EncodeBufferQueue;
		EncodeConfig							m_encodeConfig;
		bool									m_encoderInitialized;
		ID3D11Texture2D*						m_stagingFrameBuffer;
		D3D11_TEXTURE2D_DESC					m_stagingFrameBufferDesc;

		NVENCSTATUS								InitializeEncoder(DXGI_SWAP_CHAIN_DESC swapChainDesc, EncodeConfig nvEncodeConfig);
		NVENCSTATUS								AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight, DXGI_SWAP_CHAIN_DESC swapChainDesc);
		NVENCSTATUS								ReleaseIOBuffers();
		NVENCSTATUS                             FlushEncoder();
	};
}
