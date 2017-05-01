#pragma once

#include <d3d11.h>

#include "defs.h"
#include "video_helper.h"

namespace Toolkit3DLibrary
{
	class VideoTestRunner
	{
	public:
		VideoTestRunner(ID3D11Device* device, ID3D11DeviceContext* context);
		~VideoTestRunner();
		void									InitializeTest();
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
		VideoHelper*							m_videoHelper;

		// NvEncoder
		EncodeConfig							m_encodeConfig;
		bool									m_encoderInitialized;

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

	};
}
