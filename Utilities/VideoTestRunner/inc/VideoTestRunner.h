#pragma once

#include <d3d11.h>
#include "pch.h"
#include "nvEncodeAPI.h"
#include "nvCPUOPSys.h"
#include "NvHWEncoder.h"

namespace StreamingToolkit
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

	class VideoTestRunner
	{
	public:
		VideoTestRunner(ID3D11Device* device, ID3D11DeviceContext* context);
		~VideoTestRunner();
		void									InitializeTest();
		NVENCSTATUS                             Deinitialize();
		void									StartTestRunner(IDXGISwapChain* swapChain);
		void									IncrementTest();
		void									IncrementTestSuite();
		bool									IsNewTest();
		bool									TestsComplete();
		void									TestCapture();

	private:
		ID3D11Device*							m_d3dDevice;
		ID3D11DeviceContext*					m_d3dContext;
		IDXGISwapChain*							m_swapChain;

		// NvEncoder
		EncodeConfig							m_encodeConfig;
		bool									m_encoderCreated;

		CNvHWEncoder*                           m_pNvHWEncoder;
		uint32_t                                m_uEncodeBufferCount;
		EncodeOutputBuffer						m_stEOSOutputBfr;
		EncodeBuffer							m_stEncodeBuffer[MAX_ENCODE_QUEUE];
		CNvQueue<EncodeBuffer>                  m_EncodeBufferQueue;

		// TestRunner
		EncodeConfig							m_minEncodeConfig;
		EncodeConfig							m_maxEncodeConfig;
		EncodeConfig							m_stepEncodeConfig;
		bool									m_lastTest;
		bool									m_testSuiteComplete;
		bool									m_testRunComplete;
		int										m_currentSuite;
		int										m_currentFrame;
		bool								    m_initialized;
		char*									m_fileName;

		NVENCSTATUS								InitializeEncoder();
		NVENCSTATUS								AllocateIOBuffers();
		NVENCSTATUS								ReleaseIOBuffers();
		NVENCSTATUS                             FlushEncoder();
		void									GetDefaultEncodeConfig();
		NVENCSTATUS								SetEncodeProfile(int profileIndex);
		void									Capture();
	};
}
