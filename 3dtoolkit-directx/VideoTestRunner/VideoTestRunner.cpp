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
	memset(&m_minEncodeConfig, 0, sizeof(EncodeConfig));
	memset(&m_stepEncodeConfig, 0, sizeof(EncodeConfig));
	memset(&m_maxEncodeConfig, 0, sizeof(EncodeConfig));
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
				IncrementTestSuite();
				return;
			}
			IncrementTest();
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
	m_testSuiteComplete = false;
	m_testRunComplete = false;
	m_currentSuite = 0;
	m_swapChain = swapChain;
	m_fileName = "lossless.h264";

	m_currentFrame = 0;
	m_lastTest = false;
	m_videoHelper->GetDefaultEncodeConfig(m_encodeConfig);
	m_videoHelper->GetDefaultEncodeConfig(m_encodeConfig);
	m_videoHelper->SetEncodeProfile(1);
	m_encodeConfig.endFrameIdx = 300;
	m_encodeConfig.enableTemporalAQ = true;
	m_minEncodeConfig = m_encodeConfig;

	IncrementTest();
}

void VideoTestRunner::IncrementTest() {
	if (!access(m_fileName, 0) == 0) {
		if (m_encoderInitialized) {
			//Special casing first run for lossless
			if (m_currentFrame == 0)
				IncrementTestSuite();
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
	strcat(m_fileName, "kbps-");
	strcat(m_fileName, m_encodeConfig.encoderPreset);
	switch (m_encodeConfig.rcMode) {
		case NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ:  strcat(m_fileName, "-CBRLLHQ.h264"); break;
		case NV_ENC_PARAMS_RC_CBR:				strcat(m_fileName, "-CBR.h264"); break;
		case NV_ENC_PARAMS_RC_CONSTQP:			
			strcat(m_fileName, "-CONSTQP");
			strcat(m_fileName, std::to_string(m_encodeConfig.qp).c_str());
			strcat(m_fileName, ".h264");
			break; 
		case NV_ENC_PARAMS_RC_CBR_HQ:			strcat(m_fileName, "-CBRHQ.h264"); break;
		case NV_ENC_PARAMS_RC_VBR_HQ:
			strcat(m_fileName, "-VBRHQQP");
			strcat(m_fileName, std::to_string(m_encodeConfig.qp).c_str());
			strcat(m_fileName, ".h264");
			break;
		case NV_ENC_PARAMS_RC_VBR:				
			strcat(m_fileName, "-VBRQP");
			strcat(m_fileName, std::to_string(m_encodeConfig.qp).c_str());
			strcat(m_fileName, ".h264");
			break;
	}

	if (m_encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP) {
		if (m_encodeConfig.qp + 1 <= 36) {
			m_encodeConfig.qp++;
			IncrementTest();
			return;
		}
		else {
			m_encodeConfig.qp = 36;
			m_lastTest = true;
			IncrementTest();
		}
	}
	else {
		if (m_encodeConfig.bitrate + m_stepEncodeConfig.bitrate <= m_maxEncodeConfig.bitrate) {
			m_encodeConfig.bitrate += m_stepEncodeConfig.bitrate;
			IncrementTest();
			return;
		}
		else {
			m_encodeConfig.bitrate = m_maxEncodeConfig.bitrate;
			m_lastTest = true;
			IncrementTest();
		}
	}
	
	
}

void VideoTestRunner::IncrementTestSuite() {
	if (m_testRunComplete)
		return;
	m_currentFrame = 0;
	m_lastTest = false;
	m_minEncodeConfig.bitrate = 2500000;
	m_stepEncodeConfig.bitrate = 250000;
	m_maxEncodeConfig.bitrate = 10000000;

	switch (m_currentSuite) {
	case 0:
		m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
		m_minEncodeConfig.encoderPreset = "losslessHP";
	case 1: //Lowlatency CBR
		m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
		m_minEncodeConfig.encoderPreset = "lowLatencyHQ";
		break;
	case 2: //VBR HQ
		m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_VBR_HQ;
		m_minEncodeConfig.encoderPreset = "hq";
		break;
	case 3: //VBR
		m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_VBR;
		m_minEncodeConfig.encoderPreset = "hq";
		break;
	case 4: //VBR BluRay
		m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_VBR_HQ;
		m_minEncodeConfig.encoderPreset = "bluray";
		break;
	case 5: //Constant quality QP
		m_minEncodeConfig.qp = 21;
		m_minEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
		m_minEncodeConfig.encoderPreset = "lossless";
		break;
	default:
		m_testRunComplete = true;
		break;
	}
	m_currentSuite++;
	m_encodeConfig = m_minEncodeConfig;
	IncrementTest();
}

bool VideoTestRunner::IsNewTest() {
	return m_currentFrame == 0 ? true : false;
}

bool VideoTestRunner::TestsComplete() {
	return m_testRunComplete;
}