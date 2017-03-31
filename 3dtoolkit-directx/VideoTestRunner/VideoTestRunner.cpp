#pragma once

#include "pch.h"
#include "VideoTestRunner.h"
#include <string>

using namespace Toolkit3DLibrary;

// Constructor for VideoHelper.
VideoTestRunner::VideoTestRunner(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context)
{
	m_videoHelper = new VideoHelper(device, context);
	memset(&m_encodeConfig, 0, sizeof(EncodeConfig));
}

// Destructor for VideoHelper.
VideoTestRunner::~VideoTestRunner()
{
	delete m_videoHelper;
	m_encoderInitialized = false;
}

void VideoTestRunner::InitializeTest()
{
	m_encodeConfig.outputFileName = m_fileName;
	m_videoHelper->Initialize(m_swapChain, m_encodeConfig);
	m_encoderInitialized = true;
}

void VideoTestRunner::TestCapture() {
	if (m_currentFrame >= m_encodeConfig.startFrameIdx)
	{
		if (m_currentFrame < m_encodeConfig.endFrameIdx) {
			m_videoHelper->Capture();
		}
		else {
			m_currentFrame = 0;
			IncrementTestRunner();
			return;
		}
	} 
	m_currentFrame++;
}

void VideoTestRunner::StartTestRunner(IDXGISwapChain* swapChain) {
	m_testsComplete = false;
	m_swapChain = swapChain;
	m_fileName = "lossless.h264";

	m_videoHelper->GetDefaultEncodeConfig(m_encodeConfig);
	m_encodeConfig.endFrameIdx = 200;

	m_currentFrame = 0;

	memset(&m_minEncodeConfig, 0, sizeof(EncodeConfig));
	m_minEncodeConfig = m_encodeConfig;
	m_minEncodeConfig.bitrate = 1500000;
	m_minEncodeConfig.enableTemporalAQ = false;
	m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_minEncodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;

	memset(&m_maxEncodeConfig, 0, sizeof(EncodeConfig));
	m_maxEncodeConfig = m_encodeConfig;
	m_maxEncodeConfig.bitrate = 5000000;
	m_maxEncodeConfig.enableTemporalAQ = true;
	m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_maxEncodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;

	memset(&m_stepEncodeConfig, 0, sizeof(EncodeConfig));
	m_stepEncodeConfig = m_encodeConfig;
	m_stepEncodeConfig.bitrate = 250000;
	m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_stepEncodeConfig.presetGUID = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;

	IncrementTestRunner();
}

void VideoTestRunner::IncrementTestRunner() {
	if (!access(m_fileName, 0) == 0) {
		if (m_encoderInitialized) {
			m_videoHelper->Deinitialize();
			m_encoderInitialized = false;
		}
		InitializeTest();
		return;
	}
		
	//Test to see if we are incrementing from lossless to the runner iterations
	if (m_fileName == "lossless.h264") {
		//Need to reset to runner config
		m_encodeConfig = m_minEncodeConfig;
	}

	m_fileName = new char[255];
	strcpy(m_fileName, std::to_string(m_encodeConfig.bitrate / 1000).c_str());
	strcat(m_fileName, "kbps-AQ");
	strcat(m_fileName, std::to_string(m_encodeConfig.enableTemporalAQ).c_str());
	strcat(m_fileName, ".h264");

	if (m_encodeConfig.enableTemporalAQ == !m_minEncodeConfig.enableTemporalAQ) {
		m_encodeConfig.enableTemporalAQ = m_minEncodeConfig.enableTemporalAQ;
	}
	else {
		m_encodeConfig.enableTemporalAQ = !m_minEncodeConfig.enableTemporalAQ;
		IncrementTestRunner();
		return;
	}

	if (m_encodeConfig.bitrate + m_stepEncodeConfig.bitrate <= m_maxEncodeConfig.bitrate) {
		m_encodeConfig.bitrate += m_stepEncodeConfig.bitrate;
		IncrementTestRunner();
		return;
	}
	else {
		m_encodeConfig.bitrate = m_minEncodeConfig.bitrate;
	}

	if (m_encodeConfig.qp == m_minEncodeConfig.qp &&
		m_encodeConfig.bitrate == m_minEncodeConfig.bitrate) {
		m_testsComplete = true;
		return;
	}	

}

bool VideoTestRunner::IsNewTest() {
	return m_currentFrame == 0 ? true : false;
}

bool VideoTestRunner::TestsComplete() {
	return m_testsComplete;
}