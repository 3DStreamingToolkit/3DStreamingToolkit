#pragma once

// Unreferenced formal parameters in headers
#pragma warning(disable : 4100)

#include "pch.h"
#include "nvFileIO.h"
#include "nvUtils.h"
#include "VideoTestRunner.h"
#include <string>

using namespace StreamingToolkit;

// Constructor for VideoTestRunner.
VideoTestRunner::VideoTestRunner(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context),
	m_initialized(false),
	m_encoderCreated(false)
{
	if (!m_initialized) 
	{
		m_initialized = true;

#ifdef MULTITHREAD_PROTECTION
		// Enables multithread protection.
		ID3D11Multithread* multithread;
		m_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread));
		multithread->SetMultithreadProtected(true);
		multithread->Release();
#endif // MULTITHREAD_PROTECTION

		m_pNvHWEncoder = new CNvHWEncoder();
		m_encoderCreated = false;
		m_lastTest = false;
	}
	else 
	{
		return;
	}
}

// Destructor for VideoHelper.
VideoTestRunner::~VideoTestRunner()
{
	if (m_pNvHWEncoder)
	{
		m_initialized = false;
		Deinitialize();
		delete m_pNvHWEncoder;
		m_pNvHWEncoder = NULL;
	}
}

// Cleanup resources.
NVENCSTATUS VideoTestRunner::Deinitialize()
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	FlushEncoder();
	ReleaseIOBuffers();
	nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();
	return nvStatus;
}

void VideoTestRunner::StartTestRunner(IDXGISwapChain* swapChain) 
{
	//Simple check to allow TestRunner to act as singleton for multithreaded renderers
	//if (m_initialized)
	//{
	//	return;
	//}

	m_initialized = true;
	m_testSuiteComplete = false;
	m_testRunComplete = false;
	m_currentSuite = 0;
	m_swapChain = swapChain;

	memset(&m_encodeConfig, 0, sizeof(EncodeConfig));
	memset(&m_minEncodeConfig, 0, sizeof(EncodeConfig));
	memset(&m_stepEncodeConfig, 0, sizeof(EncodeConfig));
	memset(&m_maxEncodeConfig, 0, sizeof(EncodeConfig));

	m_currentFrame = 0;
	m_lastTest = false;
	GetDefaultEncodeConfig();
	m_minEncodeConfig = m_encodeConfig;

	IncrementTest();
}

void VideoTestRunner::InitializeTest()
{
	m_encodeConfig.outputFileName = m_fileName;
	if (!m_encoderCreated)
	{
		InitializeEncoder();
	}

	m_encoderCreated = true;
}

NVENCSTATUS VideoTestRunner::InitializeEncoder()
{
	if (m_encodeConfig.outputFileName)
	{
		m_encodeConfig.fOutput = fopen(m_encodeConfig.outputFileName, "wb");
		if (m_encodeConfig.fOutput == NULL)
		{
			PRINTERR("Failed to create \"%s\"\n", m_encodeConfig.outputFileName);
		}
	}

	//NV_ENC_H264_PROFILE_HIGH_444_GUID
	SetEncodeProfile(1);

	m_pNvHWEncoder->Initialize((void*)m_d3dDevice, NV_ENC_DEVICE_TYPE_DIRECTX);

	m_encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(m_encodeConfig.encoderPreset, m_encodeConfig.codec);

	//H264 level sets maximum bitrate limits.  4.1 supported by almost all mobile devices.
	m_pNvHWEncoder->m_stEncodeConfig.encodeCodecConfig.h264Config.level = NV_ENC_LEVEL_H264_41;

	//Specific mandatory settings for LOSSLESS encoding presets
	if (m_encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID ||
		m_encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_HP_GUID)
	{
		m_encodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
		m_pNvHWEncoder->m_stEncodeConfig.profileGUID = NV_ENC_H264_PROFILE_HIGH_444_GUID;
		m_pNvHWEncoder->m_stEncodeConfig.encodeCodecConfig.h264Config.qpPrimeYZeroTransformBypassFlag = 1;
	}

	// Creates the encoder.
	m_pNvHWEncoder->CreateEncoder(&m_encodeConfig);

	m_uEncodeBufferCount = m_encodeConfig.numB + 4;

	AllocateIOBuffers();

	return NV_ENC_SUCCESS;
}

void VideoTestRunner::GetDefaultEncodeConfig()
{
	m_encodeConfig.outputFileName = m_fileName = "lossless.h264";

	// Gets the swap chain desc.
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	m_swapChain->GetDesc(&swapChainDesc);
	m_encodeConfig.width = swapChainDesc.BufferDesc.Width;
	m_encodeConfig.height = swapChainDesc.BufferDesc.Height;

	//Unused by nvEncode.
	m_encodeConfig.endFrameIdx = INT_MAX;
	m_encodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
	m_encodeConfig.encoderPreset = "lossless";

	//Infinite needed for low latency encoding.
	m_encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;

	//Unused by nvEncode.
	m_encodeConfig.startFrameIdx = 0;
#ifdef _DEBUG
	m_encodeConfig.endFrameIdx = 300;
#else // _DEBUG
	m_encodeConfig.endFrameIdx = 1000;
#endif // _DEBUG

	//DeviceType 1 = Force CUDA.
	m_encodeConfig.deviceType = 0;

	//Only supported codec for WebRTC.
	m_encodeConfig.codec = NV_ENC_H264;
	m_encodeConfig.fps = 60;

	//In bits per second - ignored for lossless presets.
	m_encodeConfig.bitrate = 1032517;

	//Quantization Parameter - must be 0 for lossless.
	m_encodeConfig.qp = 0;

	//Must be set to frame.
	m_encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

	//Initial QP factors for bitrate spinup.
	m_encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
	m_encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;
	m_encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
	m_encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET;
	m_encodeConfig.intraRefreshEnableFlag = true;
	m_encodeConfig.intraRefreshPeriod = 30;
	m_encodeConfig.intraRefreshDuration = 3;

	// Enable temporal Adaptive Quantization
	// Shifts quantization matrix based on complexity of frame over time
	m_encodeConfig.enableTemporalAQ = false;

	//Need this to be able to recover from stream drops
	//Client needs to send back a last good timestamp, and we call
	//NvEncInvalidateRefFrames(encoder,timestamp) to reissue I frame
	m_encodeConfig.invalidateRefFramesEnableFlag = true;
}

// Captures frame buffer from the swap chain.
void VideoTestRunner::Capture()
{
	// Try to process the pending input buffers.
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	EncodeBuffer* pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();

	if (!pEncodeBuffer)
	{
		pEncodeBuffer = m_EncodeBufferQueue.GetPending();
		m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);

		// UnMap the input buffer after frame done
		if (pEncodeBuffer->stInputBfr.hInputSurface)
		{
			nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
			pEncodeBuffer->stInputBfr.hInputSurface = NULL;
		}

		pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
	}

	ID3D11Texture2D* frameBuffer = nullptr;
	HRESULT hr = m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer));

	// Copies the frame buffer to the encode input buffer.
	m_d3dContext->CopyResource(pEncodeBuffer->stInputBfr.pARGBSurface, frameBuffer);
	frameBuffer->Release();
	nvStatus = m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		PRINTERR("Failed to Map input buffer %p\n", pEncodeBuffer->stInputBfr.hInputSurface);
		return;
	}

	// Encoding.
	if (SUCCEEDED(hr))
	{
		nvStatus = m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, m_encodeConfig.width, m_encodeConfig.height);
		if (nvStatus != NV_ENC_SUCCESS  && nvStatus != NV_ENC_ERR_NEED_MORE_INPUT)
		{
			return;
		}
	}
}

NVENCSTATUS VideoTestRunner::AllocateIOBuffers()
{
	ID3D11Texture2D* pVPSurfaces[16];

	// Gets the swap chain desc.
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	m_swapChain->GetDesc(&swapChainDesc);

	// Initializes the encode buffer queue.
	m_EncodeBufferQueue.Initialize(m_stEncodeBuffer, m_uEncodeBufferCount);

	// Finds the suitable format for buffer.
	DXGI_FORMAT format = swapChainDesc.BufferDesc.Format;
	if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
	{
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
	}

	for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
	{
		// Initializes the input buffer, backed by ID3D11Texture2D*.
		D3D11_TEXTURE2D_DESC desc = { 0 };
		desc.ArraySize = 1;
		desc.Format = format;
		desc.Width = m_encodeConfig.width;
		desc.Height = m_encodeConfig.height;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		m_d3dDevice->CreateTexture2D(&desc, nullptr, &pVPSurfaces[i]);

		// Registers the input buffer with NvEnc.
		CHECK_NV_FAILED(m_pNvHWEncoder->NvEncRegisterResource(
			NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX,
			(void*)pVPSurfaces[i],
			m_encodeConfig.width,
			m_encodeConfig.height,
			m_stEncodeBuffer[i].stInputBfr.uARGBStride,
			&m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource));

		// Maps the buffer format to the relevant NvEnc encoder format
		switch (format)
		{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;
			break;

		case DXGI_FORMAT_R10G10B10A2_UNORM:
			if (m_pNvHWEncoder->m_stEncodeConfig.encodeCodecConfig.h264Config.qpPrimeYZeroTransformBypassFlag == 1)
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_YUV444_10BIT;
			else
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_YUV420_10BIT;
			break;

		default:
			m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ABGR;
			break;
		}

		m_stEncodeBuffer[i].stInputBfr.dwWidth = m_encodeConfig.width;
		m_stEncodeBuffer[i].stInputBfr.dwHeight = m_encodeConfig.height;
		m_stEncodeBuffer[i].stInputBfr.pARGBSurface = pVPSurfaces[i];

		// Initializes the output buffer.
		CHECK_NV_FAILED(m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer));
		m_stEncodeBuffer[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;

		// Registers for the output event.
		CHECK_NV_FAILED(m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEncodeBuffer[i].stOutputBfr.hOutputEvent));
		m_stEncodeBuffer[i].stOutputBfr.bWaitOnEvent = true;
	}

	m_stEOSOutputBfr.bEOSFlag = TRUE;

	// Registers for the output event.
	CHECK_NV_FAILED(m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEOSOutputBfr.hOutputEvent));

	return NV_ENC_SUCCESS;
}

NVENCSTATUS VideoTestRunner::SetEncodeProfile(int profileIndex)
{
	GUID choice;
	switch (profileIndex)
	{
		//Main should be used for most cases
		case 1:
			choice = NV_ENC_H264_PROFILE_MAIN_GUID;
			break;

			//High 444 is only for lossless encode profile
		case 2:
			choice = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
			break;

			//Enables stereo frame packing
		case 3:
			choice = NV_ENC_H264_PROFILE_STEREO_GUID;
			break;

			//Fallback to auto, avoid this.
		case 0:
		default: choice = NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;
			break;
	}

	m_pNvHWEncoder->m_stEncodeConfig.profileGUID = choice;
	return NV_ENC_SUCCESS;
}

NVENCSTATUS VideoTestRunner::ReleaseIOBuffers()
{
	for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
	{
		SAFE_RELEASE(m_stEncodeBuffer[i].stInputBfr.pARGBSurface);

		m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
		m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer = NULL;

		m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
		CloseHandle(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
		m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
	}

	if (m_stEOSOutputBfr.hOutputEvent)
	{
		m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEOSOutputBfr.hOutputEvent);
		CloseHandle(m_stEOSOutputBfr.hOutputEvent);
		m_stEOSOutputBfr.hOutputEvent = NULL;
	}

	return NV_ENC_SUCCESS;
}

NVENCSTATUS VideoTestRunner::FlushEncoder()
{
	NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncFlushEncoderQueue(m_stEOSOutputBfr.hOutputEvent);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		assert(0);
		return nvStatus;
	}

	EncodeBuffer *pEncodeBuffer = m_EncodeBufferQueue.GetPending();
	while (pEncodeBuffer)
	{
		m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
		pEncodeBuffer = m_EncodeBufferQueue.GetPending();

		// UnMap the input buffer after frame is done.
		if (pEncodeBuffer && pEncodeBuffer->stInputBfr.hInputSurface)
		{
			nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
			pEncodeBuffer->stInputBfr.hInputSurface = NULL;
		}
	}

	if (WaitForSingleObject(m_stEOSOutputBfr.hOutputEvent, 500) != WAIT_OBJECT_0)
	{
		assert(0);
		nvStatus = NV_ENC_ERR_GENERIC;
	}

	return nvStatus;
}

void VideoTestRunner::TestCapture() 
{
	if (m_encoderCreated) 
	{
		if (m_currentFrame >= m_encodeConfig.startFrameIdx)
		{
			if (m_currentFrame < m_encodeConfig.endFrameIdx) 
			{
				Capture();
			}
			else 
			{
				m_currentFrame = 0;
				if (m_lastTest) 
				{
					IncrementTestSuite();
					return;
				}

				IncrementTest();
				return;
			}
		}

		m_currentFrame++;
	}
}

void VideoTestRunner::IncrementTest() 
{
	if (!access(m_fileName, 0) == 0) 
	{
		if (m_encoderCreated) 
		{
			Deinitialize();
			m_encoderCreated = false;
		}

		InitializeTest();
		return;
	}

	if (m_lastTest) 
	{
		return;
	}

	//Test to see if we are incrementing from lossless to the runner iterations
	if (strcmp(m_fileName,"lossless.h264") == 0) 
	{
		//Need to reset to runner config
		m_encodeConfig = m_minEncodeConfig;
	}

	m_fileName = new char[255];
	if (m_encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP) 
	{
		strcpy(m_fileName, m_encodeConfig.encoderPreset);
	}
	else 
	{
		strcpy(m_fileName, std::to_string(m_encodeConfig.bitrate / 1000).c_str());
		strcat(m_fileName, "kbps-");
		strcat(m_fileName, m_encodeConfig.encoderPreset);
	}
	
	switch (m_encodeConfig.rcMode) 
	{
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

	if (m_encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP) 
	{
		if (m_encodeConfig.qp + 1 <= 36) 
		{
			m_encodeConfig.qp++;
			IncrementTest();
			return;
		}
		else 
		{
			m_encodeConfig.qp = 36;
			m_lastTest = true;
			IncrementTest();
		}
	}
	else 
	{
		if (m_encodeConfig.bitrate + m_stepEncodeConfig.bitrate <= m_maxEncodeConfig.bitrate) 
		{
			m_encodeConfig.bitrate += m_stepEncodeConfig.bitrate;
			IncrementTest();
			return;
		}
		else 
		{
			m_encodeConfig.bitrate = m_maxEncodeConfig.bitrate;
			m_lastTest = true;
			IncrementTest();
		}
	}
}

void VideoTestRunner::IncrementTestSuite() 
{
	if (m_testRunComplete)
	{
		return;
	}

	m_currentFrame = 0;
	m_lastTest = false;
	m_minEncodeConfig.bitrate = 2500000;
	m_stepEncodeConfig.bitrate = 250000;
	m_maxEncodeConfig.bitrate = 10000000;

	switch (m_currentSuite) 
	{
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

bool VideoTestRunner::IsNewTest() 
{
	return m_currentFrame == 0 ? true : false;
}

bool VideoTestRunner::TestsComplete() 
{
	return m_testRunComplete;
}
