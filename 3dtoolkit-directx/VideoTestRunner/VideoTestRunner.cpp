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
}

// Destructor for VideoHelper.
VideoTestRunner::~VideoTestRunner()
{
	delete m_videoHelper;
}

void VideoTestRunner::InitializeTest()
{
	m_videoHelper->Initialize(m_swapChain, m_encodeConfig);
	m_encoderInitialized = true;
}

void VideoTestRunner::TestCapture() {
	if (m_currentFrame < m_encodeConfig.endFrameIdx) {
		m_videoHelper->Capture();
		m_currentFrame++;
	}
	else {
		m_currentFrame = 0;
		IncrementTestRunner();
	}
}

void VideoTestRunner::StartTestRunner(IDXGISwapChain* swapChain) {
	m_testsComplete = false;
	m_swapChain = swapChain;

	EncodeConfig m_encodeConfig;
	memset(&m_encodeConfig, 0, sizeof(EncodeConfig));

	m_videoHelper->GetDefaultEncodeConfig(m_encodeConfig);
	m_encodeConfig.outputFileName = "lossless.h264";
	m_encodeConfig.bitrate = 100000000;
	m_encodeConfig.endFrameIdx = 500;
	m_encodeConfig.presetGUID = NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID;
	m_encodeConfig.fOutput = fopen(m_encodeConfig.outputFileName, "wb");

	m_currentFrame = m_encodeConfig.startFrameIdx;

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

void VideoTestRunner::IncrementTestRunner() {
	if (!m_testsComplete) {
		if (!access("lossless.h264", 0) == 0) {
			InitializeTest();
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
				if (m_encoderInitialized) {
					delete m_videoHelper;
					m_videoHelper = new VideoHelper(m_d3dDevice, m_d3dContext);
				}
				InitializeTest();
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

bool VideoTestRunner::IsNewTest() {
	return m_currentFrame == 0 ? true : false;
}

bool VideoTestRunner::TestsComplete() {
	return m_testsComplete;
}