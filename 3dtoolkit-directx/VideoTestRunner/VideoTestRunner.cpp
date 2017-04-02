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
	m_initialized = false;
	m_encoderInitialized = false;
	m_lastTest = false;
}

// Destructor for VideoHelper.
VideoTestRunner::~VideoTestRunner()
{
	delete m_videoHelper;
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
			if (m_lastTest) {
				m_testsComplete = true;
				m_lastTest = false;
				return;
			}
			IncrementTestRunner();
			return;
		}
	} 
	m_currentFrame++;
}

void VideoTestRunner::StartTestRunner(IDXGISwapChain* swapChain) {
	//Simple check to allow TestRunner to act as singleton for multithreaded renderers
	if (m_initialized)
		return;
	m_initialized = true;
	m_testsComplete = false;
	m_swapChain = swapChain;
	m_fileName = "lossless.h264";

	m_videoHelper->GetDefaultEncodeConfig(m_encodeConfig);
	m_encodeConfig.endFrameIdx = 300;

	m_currentFrame = 0;

	memset(&m_minEncodeConfig, 0, sizeof(EncodeConfig));
	m_minEncodeConfig = m_encodeConfig;
	m_minEncodeConfig.bitrate = 1500000;
	m_minEncodeConfig.enableTemporalAQ = true;
	m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_minEncodeConfig.encoderPreset = "lowLatencyHQ";
	m_videoHelper->SetEncodeProfile(1);

	memset(&m_maxEncodeConfig, 0, sizeof(EncodeConfig));
	m_maxEncodeConfig = m_encodeConfig;
	m_maxEncodeConfig.bitrate = 10000000;
	m_maxEncodeConfig.enableTemporalAQ = true;
	m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_minEncodeConfig.encoderPreset = "lowLatencyHQ";

	memset(&m_stepEncodeConfig, 0, sizeof(EncodeConfig));
	m_stepEncodeConfig = m_encodeConfig;
	m_stepEncodeConfig.bitrate = 250000;
	m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	m_minEncodeConfig.encoderPreset = "lowLatencyHQ";

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
	if (m_lastTest) {
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
	strcat(m_fileName, "-");
	strcat(m_fileName, m_encodeConfig.encoderPreset);
	strcat(m_fileName, "-CBR-LL-HQ.h264");

	if (m_encodeConfig.enableTemporalAQ == 0) {
		m_encodeConfig.enableTemporalAQ = 1;
	}
	else {
		m_encodeConfig.enableTemporalAQ = 0;
		IncrementTestRunner();
		return;
	}

	if (m_encodeConfig.bitrate + m_stepEncodeConfig.bitrate <= m_maxEncodeConfig.bitrate) {
		m_encodeConfig.bitrate += m_stepEncodeConfig.bitrate;
		IncrementTestRunner();
		return;
	}
	else {
		m_encodeConfig.bitrate = m_maxEncodeConfig.bitrate;
		m_lastTest = true;
		IncrementTestRunner();
	}
}

bool VideoTestRunner::IsNewTest() {
	return m_currentFrame == 0 ? true : false;
}

bool VideoTestRunner::TestsComplete() {
	return m_testsComplete;
}