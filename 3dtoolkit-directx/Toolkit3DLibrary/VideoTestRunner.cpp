#pragma once

#include "pch.h"
#ifdef _WIN32
	#include <io.h> 
	#define access    _access_s
#else
	#include <unistd.h>
#endif
#include "VideoHelper.h"
#include "nvFileIO.h"
#include "nvUtils.h"
#include <string>;

using namespace Toolkit3DLibrary;

//Constructor for Video Test Runner
void VideoHelper::InitializeTestRunner()
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	m_swapChain->GetDesc(&swapChainDesc);

	nvStatus = m_pNvHWEncoder->Initialize((void*)m_d3dDevice, NV_ENC_DEVICE_TYPE_DIRECTX);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return;
	}

	m_encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(m_encodeConfig.encoderPreset, m_encodeConfig.codec);

	// Prints config info to console.
	PrintConfig(m_encodeConfig);

	// Creates the encoder.
	nvStatus = m_pNvHWEncoder->CreateEncoder(&m_encodeConfig);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return;
	}

	m_uEncodeBufferCount = m_encodeConfig.numB + 4;

	nvStatus = AllocateIOBuffers(m_encodeConfig.width, m_encodeConfig.height, swapChainDesc);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return;
	}
	m_encoderInitialized = true;
}

void VideoHelper::TestCapture() {
	if (m_currentFrame < m_encodeConfig.endFrameIdx) {
		Capture();
		m_currentFrame++;
	}
	else {
		m_currentFrame = 0;
		IncrementTestRunner();
	}
}

void VideoHelper::StartTestRunner(IDXGISwapChain* swapChain) {
	m_testsComplete = false;
	EncodeConfig encodingSettings;
	memset(&encodingSettings, 0, sizeof(EncodeConfig));
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChain->GetDesc(&swapChainDesc);
	m_swapChain = swapChain;


	encodingSettings.outputFileName = "lossless.h264";
	encodingSettings.width = swapChainDesc.BufferDesc.Width;
	encodingSettings.height = swapChainDesc.BufferDesc.Height;
	encodingSettings.bitrate = 100000000;
	encodingSettings.startFrameIdx = 0;
	encodingSettings.endFrameIdx = 500;
	//Infinite needed for low latency encoding
	encodingSettings.gopLength = NVENC_INFINITE_GOPLENGTH;
	encodingSettings.deviceType = 0;
	encodingSettings.codec = NV_ENC_H264;
	encodingSettings.fps = 60;
	encodingSettings.presetGUID = NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID;
	encodingSettings.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	encodingSettings.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	encodingSettings.fOutput = fopen(encodingSettings.outputFileName, "wb");

	SetEncodeConfig(encodingSettings);
	m_currentFrame = encodingSettings.startFrameIdx;

	memset(&m_minEncodeConfig, 0, sizeof(EncodeConfig));
	m_minEncodeConfig = m_encodeConfig;
	m_minEncodeConfig.bitrate = 250000;
	m_minEncodeConfig.qp = 40;
	m_minEncodeConfig.enableTemporalAQ = false;
	m_minEncodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;

	memset(&m_maxEncodeConfig, 0, sizeof(EncodeConfig));
	m_maxEncodeConfig = m_encodeConfig;
	m_maxEncodeConfig.bitrate = 5000000;
	m_maxEncodeConfig.qp = 51;
	m_maxEncodeConfig.enableTemporalAQ = true;
	m_maxEncodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;

	memset(&m_stepEncodeConfig, 0, sizeof(EncodeConfig));
	m_stepEncodeConfig = m_encodeConfig;
	m_stepEncodeConfig.bitrate = 250000;
	m_stepEncodeConfig.qp = 5;
	m_stepEncodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;

	IncrementTestRunner();
}

void VideoHelper::IncrementTestRunner() {
	if (!m_testsComplete) {
		if (!access("lossless.h264", 0) == 0) {
			InitializeTestRunner();
		}
		else {
			//Test to see if we are incrementing from lossless to the runner iterations
			if (m_encodeConfig.outputFileName == "lossless.h264") {
				//Need to reset to runner config
				m_encodeConfig = m_minEncodeConfig;
			}
			char outFileName[255];
			strcpy(outFileName, "qp");
			strcat(outFileName, std::to_string(m_encodeConfig.qp).c_str());
			strcat(outFileName, "-bitrate");
			strcat(outFileName, std::to_string(m_encodeConfig.bitrate / 1000).c_str());
			strcat(outFileName, "kbps-AQ");
			strcat(outFileName, std::to_string(m_encodeConfig.enableTemporalAQ).c_str());
			strcat(outFileName, ".h264");
			m_encodeConfig.outputFileName = outFileName;
			//If file already exists, don't rerun it.
			if (!access(outFileName, 0) == 0) {
				m_encodeConfig.fOutput = fopen(m_encodeConfig.outputFileName, "wb");
				if (m_encoderInitialized)
					Deinitialize();
				InitializeTestRunner();
			}
			else {
				if (m_encodeConfig.enableTemporalAQ == !m_minEncodeConfig.enableTemporalAQ) {
					m_encodeConfig.enableTemporalAQ = m_minEncodeConfig.enableTemporalAQ;
				}
				else {
					m_encodeConfig.enableTemporalAQ = !m_minEncodeConfig.enableTemporalAQ;
					IncrementTestRunner();
				}

				if (m_encodeConfig.bitrate + m_stepEncodeConfig.bitrate <= m_maxEncodeConfig.bitrate) {
					m_encodeConfig.bitrate += m_stepEncodeConfig.bitrate;
					IncrementTestRunner();
				}
				else {
					m_encodeConfig.bitrate = m_minEncodeConfig.bitrate;
				}

				if (m_encodeConfig.qp + m_stepEncodeConfig.qp <= m_maxEncodeConfig.qp) {
					m_encodeConfig.qp += m_stepEncodeConfig.qp;
					IncrementTestRunner();
				}
				else {
					m_encodeConfig.qp = m_minEncodeConfig.qp;
				}

				if (m_encodeConfig.qp == m_minEncodeConfig.qp &&
					m_encodeConfig.bitrate == m_minEncodeConfig.bitrate) {
					m_testsComplete = true;
					return;
				}

			}
		}
	}
}

bool VideoHelper::IsNewTest() {
	return m_currentFrame == 0 ? true : false;
}

bool VideoHelper::TestsComplete() {
	return m_testsComplete;
}