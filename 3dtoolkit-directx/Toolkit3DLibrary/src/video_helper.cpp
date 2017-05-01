#pragma once

#include "pch.h"

<<<<<<< HEAD:3dtoolkit-directx/Toolkit3DLibrary/src/VideoHelper.cpp
#include "VideoHelper.h"
=======
#include "video_helper.h"
#include "nvFileIO.h"
#include "nvUtils.h"
>>>>>>> refs/remotes/origin/master:3dtoolkit-directx/Toolkit3DLibrary/src/video_helper.cpp

using namespace Toolkit3DLibrary;

// Constructor for VideoHelper.
VideoHelper::VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context)
{

#ifdef MULTITHREAD_PROTECTION
	// Enables multithread protection.
	ID3D11Multithread* multithread;
	m_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
#endif // MULTITHREAD_PROTECTION

#ifdef USE_WEBRTC_NVENCODE
	webrtc::H264EncoderImpl::SetDevice(device);
	webrtc::H264EncoderImpl::SetContext(context);
#endif // USE_WEBRTC_NVENCODE
}

// Destructor for VideoHelper.
VideoHelper::~VideoHelper()
{
	if (m_pNvHWEncoder)
	{
		Deinitialize();
		delete m_pNvHWEncoder;
		m_pNvHWEncoder = NULL;
	}

	SAFE_RELEASE(m_stagingFrameBuffer);
}

// Initializes the swap chain and IO buffers.
NVENCSTATUS VideoHelper::Initialize(IDXGISwapChain* swapChain, char* outputFile, bool initEncoder)
{
	// Caches the pointer to the swap chain.
	m_swapChain = swapChain;

	// Gets the swap chain desc.
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChain->GetDesc(&swapChainDesc);	
	
	// Initializes the staging frame buffer, backed by ID3D11Texture2D*.
	m_stagingFrameBufferDesc = { 0 };
	m_stagingFrameBufferDesc.ArraySize = 1;
	m_stagingFrameBufferDesc.Format = swapChainDesc.BufferDesc.Format;
	m_stagingFrameBufferDesc.Width = swapChainDesc.BufferDesc.Width;
	m_stagingFrameBufferDesc.Height = swapChainDesc.BufferDesc.Height;
	m_stagingFrameBufferDesc.MipLevels = 1;
	m_stagingFrameBufferDesc.SampleDesc.Count = 1;
	m_stagingFrameBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	m_stagingFrameBufferDesc.Usage = D3D11_USAGE_STAGING;
	m_d3dDevice->CreateTexture2D(&m_stagingFrameBufferDesc, nullptr, &m_stagingFrameBuffer);
	
	return NV_ENC_SUCCESS;

	// Initializes the NvEncoder.
	if (outputFile != nullptr || initEncoder)
	{
		 m_pNvHWEncoder = new CNvHWEncoder();
		 memset(&m_encodeConfig, 0, sizeof(EncodeConfig));

		// Relative path
		m_encodeConfig.outputFileName = outputFile;
		GetDefaultEncodeConfig(m_encodeConfig);
		return InitializeEncoder(swapChainDesc, m_encodeConfig);
	}
	
	return NV_ENC_SUCCESS;
}

NVENCSTATUS VideoHelper::Initialize(IDXGISwapChain* swapChain, EncodeConfig nvEncodeConfig)
{
	// Caches the pointer to the swap chain.
	m_swapChain = swapChain;

	// Gets the swap chain desc.
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChain->GetDesc(&swapChainDesc);

	m_pNvHWEncoder = new CNvHWEncoder();
	memset(&m_encodeConfig, 0, sizeof(EncodeConfig));

	return InitializeEncoder(swapChainDesc, nvEncodeConfig);
}

NVENCSTATUS VideoHelper::InitializeEncoder(DXGI_SWAP_CHAIN_DESC swapChainDesc, EncodeConfig nvEncodeConfig)
{
	return NV_ENC_SUCCESS;

	m_encodeConfig = nvEncodeConfig;
	m_encodeConfig.width = swapChainDesc.BufferDesc.Width;
	m_encodeConfig.height = swapChainDesc.BufferDesc.Height;
	if (m_encodeConfig.outputFileName)
	{
		m_encodeConfig.fOutput = fopen(m_encodeConfig.outputFileName, "wb");
		if (m_encodeConfig.fOutput == NULL)
		{
			PRINTERR("Failed to create \"%s\"\n", m_encodeConfig.outputFileName);
		}
	}

	CHECK_NV_FAILED(m_pNvHWEncoder->Initialize((void*)m_d3dDevice, NV_ENC_DEVICE_TYPE_DIRECTX));

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

	if (nvEncodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR_HQ)
	{
		m_encodeConfig.rcMode = NV_ENC_PARAMS_RC_VBR_HQ;
	}

	// Prints config info to console.
	PrintConfig(m_encodeConfig);

	// Creates the encoder.
	CHECK_NV_FAILED(m_pNvHWEncoder->CreateEncoder(&m_encodeConfig));

	m_uEncodeBufferCount = m_encodeConfig.numB + 4;

	CHECK_NV_FAILED(AllocateIOBuffers(m_encodeConfig.width, m_encodeConfig.height, swapChainDesc));

	return NV_ENC_SUCCESS;
}

void VideoHelper::GetDefaultEncodeConfig(EncodeConfig &nvEncodeConfig) 
{
	//In bits per second - ignored for lossless presets.
	nvEncodeConfig.bitrate = 1032517;

	//Unused by nvEncode.
	nvEncodeConfig.startFrameIdx = 0;

	//Unused by nvEncode.
	nvEncodeConfig.endFrameIdx = INT_MAX;
	nvEncodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
	nvEncodeConfig.encoderPreset = "lowLatencyHQ";

	//Infinite needed for low latency encoding.
	nvEncodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;

	//DeviceType 1 = Force CUDA.
	nvEncodeConfig.deviceType = 0;

	//Only supported codec for WebRTC.
	nvEncodeConfig.codec = NV_ENC_H264;
	nvEncodeConfig.fps = 30;

	//Quantization Parameter - must be 0 for lossless.
	nvEncodeConfig.qp = 34;

	//Must be set to frame.
	nvEncodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

	//Initial QP factors for bitrate spinup.
	nvEncodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
	nvEncodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;
	nvEncodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
	nvEncodeConfig.b_quant_offset = DEFAULT_B_QOFFSET;
	nvEncodeConfig.intraRefreshEnableFlag = true;
	nvEncodeConfig.intraRefreshPeriod = 30;
	nvEncodeConfig.intraRefreshDuration = 3;

	// Enable temporal Adaptive Quantization
	// Shifts quantization matrix based on complexity of frame over time
	nvEncodeConfig.enableTemporalAQ = false;

	//Need this to be able to recover from stream drops
	//Client needs to send back a last good timestamp, and we call
	//NvEncInvalidateRefFrames(encoder,timestamp) to reissue I frame
	nvEncodeConfig.invalidateRefFramesEnableFlag = true;

	//NV_ENC_H264_PROFILE_HIGH_444_GUID
	SetEncodeProfile(2);
}

// Cleanup resources.
NVENCSTATUS VideoHelper::Deinitialize()
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	FlushEncoder();
	ReleaseIOBuffers();
	nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();
	return nvStatus;
}

void VideoHelper::GetHeightAndWidth(int* width, int* height)
{
	*width = m_stagingFrameBufferDesc.Width;
	*height = m_stagingFrameBufferDesc.Height;
}

ID3D11Texture2D* VideoHelper::Capture2DTexture(int* width, int* height)
{
	// Try to process the pending input buffers.
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	ID3D11Texture2D* frameBuffer = nullptr;
	HRESULT hr = m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer));

	*width = m_stagingFrameBufferDesc.Width;
	*height = m_stagingFrameBufferDesc.Height;

	return frameBuffer;
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture()
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

// Captures frame buffer from the swap chain.
void VideoHelper::Capture(void** buffer, int* size, int* width, int* height)
{
	// Gets the frame buffer from the swap chain.
	ID3D11Texture2D* frameBuffer = nullptr;
	CHECK_HR_FAILED(m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer)));

	// Copies the frame buffer to the encode input buffer.
	m_d3dContext->CopyResource(m_stagingFrameBuffer, frameBuffer);
	frameBuffer->Release();

	// Accesses the frame buffer.
	D3D11_TEXTURE2D_DESC desc;
	m_stagingFrameBuffer->GetDesc(&desc);
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_d3dContext->Map(m_stagingFrameBuffer, 0, D3D11_MAP_READ, 0, &mapped)))
	{
		*buffer = mapped.pData;
		*size = mapped.RowPitch * desc.Height;
		*width = desc.Width;
		*height = desc.Height;
	}

	m_d3dContext->Unmap(m_stagingFrameBuffer, 0);
}

// Captures encoded frames from NvEncoder.
void VideoHelper::GetEncodedFrame(void** buffer, int* size, int* width, int* height)
{
	// Accesses the frame buffer.
	D3D11_TEXTURE2D_DESC desc;
	m_stagingFrameBuffer->GetDesc(&desc);
	*width = desc.Width;
	*height = desc.Height;
	*buffer = m_pNvHWEncoder->m_lockBitstreamData.bitstreamBufferPtr;
	*size = m_pNvHWEncoder->m_lockBitstreamData.bitstreamSizeInBytes;
}

NVENCSTATUS VideoHelper::AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight, DXGI_SWAP_CHAIN_DESC swapChainDesc)
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	ID3D11Texture2D* pVPSurfaces[16];

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
		desc.Width = uInputWidth;
		desc.Height = uInputHeight;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		m_d3dDevice->CreateTexture2D(&desc, nullptr, &pVPSurfaces[i]);

		// Registers the input buffer with NvEnc.
		CHECK_NV_FAILED(m_pNvHWEncoder->NvEncRegisterResource(
			NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX,
			(void*)pVPSurfaces[i],
			uInputWidth,
			uInputHeight,
			m_stEncodeBuffer[i].stInputBfr.uARGBStride,
			&m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource));

		// Maps the buffer format to the relevant NvEnc encoder format
		switch (format)
		{
			case DXGI_FORMAT_B8G8R8A8_UNORM:
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;
				break;

			case DXGI_FORMAT_R10G10B10A2_UNORM:
				if(m_pNvHWEncoder->m_stEncodeConfig.encodeCodecConfig.h264Config.qpPrimeYZeroTransformBypassFlag == 1)
					m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_YUV444_10BIT;
				else	
					m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_YUV420_10BIT;
				break;

			default:
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ABGR;
				break;
		}

		m_stEncodeBuffer[i].stInputBfr.dwWidth = uInputWidth;
		m_stEncodeBuffer[i].stInputBfr.dwHeight = uInputHeight;
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

NVENCSTATUS VideoHelper::SetEncodeProfile(int profileIndex) 
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

NVENCSTATUS VideoHelper::ReleaseIOBuffers()
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

NVENCSTATUS VideoHelper::FlushEncoder()
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

void VideoHelper::PrintConfig(EncodeConfig encodeConfig)
{
	printf("         output          : \"%s\"\n", encodeConfig.outputFileName);
	printf("         codec           : \"%s\"\n", encodeConfig.codec == NV_ENC_HEVC ? "HEVC" : "H264");
	printf("         size            : %dx%d\n", encodeConfig.width, encodeConfig.height);
	printf("         bitrate         : %d bits/sec\n", encodeConfig.bitrate);
	printf("         vbvMaxBitrate   : %d bits/sec\n", encodeConfig.vbvMaxBitrate);
	printf("         vbvSize         : %d bits\n", encodeConfig.vbvSize);
	printf("         fps             : %d frames/sec\n", encodeConfig.fps);
	printf("         rcMode          : %s\n", encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP ? "CONSTQP" :
		encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR ? "VBR" :
		encodeConfig.rcMode == NV_ENC_PARAMS_RC_CBR ? "CBR" :
		encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR_MINQP ? "VBR MINQP" :
		encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_QUALITY ? "TWO_PASS_QUALITY" :
		encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP ? "TWO_PASS_FRAMESIZE_CAP" :
		encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_VBR ? "TWO_PASS_VBR" : "UNKNOWN");

	if (encodeConfig.gopLength == NVENC_INFINITE_GOPLENGTH)
	{
		printf("         goplength       : INFINITE GOP \n");
	}
	else
	{
		printf("         goplength       : %d \n", encodeConfig.gopLength);
	}

	printf("         B frames        : %d \n", encodeConfig.numB);
	printf("         QP              : %d \n", encodeConfig.qp);
	printf("         preset          : %s\n", (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HQ_GUID) ? "LOW_LATENCY_HQ" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HP_GUID) ? "LOW_LATENCY_HP" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_HQ_GUID) ? "HQ_PRESET" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_HP_GUID) ? "HP_PRESET" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_HP_GUID) ? "LOSSLESS_HP" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID) ? "LOW_LATENCY_DEFAULT" : "DEFAULT");
	
	printf("\n");
}
