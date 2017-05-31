///
/// History:
/// 08 Mar 2015	Aaron Clauson (aaron@sipsorcery.com)		Created.
/// 23 Apr 2017 Phong Cao (thcao@microsoft.com)				Updated for UWP. Fixed memory leaks.
///
/// License: Public

#include "pch.h"

#include "VideoDecoder.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

using std::ios;
using std::ifstream;
using namespace WebRTCDirectXClientComponent;

EXTERN_C const CLSID CLSID_CMSH264DecoderMFT;

VideoDecoder::VideoDecoder() :
	m_pSourceResolver(nullptr),
	m_pSource(nullptr),
	m_pMediaFileSource(nullptr),
	m_pVideoReaderAttributes(nullptr),
	m_pSourceReader(nullptr),
	m_objectType(MF_OBJECT_INVALID),
	m_pFileVideoMediaType(nullptr),
	m_pDecTransformUnk(nullptr),
	m_pDecoderTransform(nullptr),
	m_pDecInputMediaType(nullptr),
	m_pDecOutputMediaType(nullptr),
	m_mftStatus(0),
	m_processOutputStatus(0),
	m_pVideoSample(nullptr),
	m_streamIndex(0),
	m_flags(0),
	m_bufferCount(0),
	m_llVideoTimeStamp(0),
	m_llSampleDuration(0),
	m_yuvVideoTimeStamp(0),
	m_yuvSampleDuration(0),
	m_mftProcessInput(S_OK),
	m_mftProcessOutput(S_OK),
	m_pOutSample(nullptr),
	m_pBuffer(nullptr),
	m_pReConstructedBuffer(nullptr),
	m_sampleCount(0),
	m_mftOutFlags(0),
	m_sampleFlags(0),
	m_reconVideoTimeStamp(0),
	m_reconSampleDuration(0),
	m_reconSampleFlags(0),
	m_pInBuffer(nullptr),
	m_pInputBuffer(nullptr),
	m_iOffset(0)
{
}

VideoDecoder::~VideoDecoder()
{
	Deinitialize();
}

void VideoDecoder::Initialize(int width, int height)
{
	m_pInputBuffer = new char[INPUT_BUFFER_SIZE];
	memset(m_pInputBuffer, 0, INPUT_BUFFER_SIZE);

	MFStartup(MF_VERSION);

	CHECK_HR(MFCreateMediaType(&m_pFileVideoMediaType), "Failed to create media type");

	// Create H.264 decoder.
	CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,
		IID_IUnknown, (void**)&m_pDecTransformUnk), "Failed to create H264 decoder MFT.\n");

	CHECK_HR(m_pDecTransformUnk->QueryInterface(
		IID_PPV_ARGS(&m_pDecoderTransform)),
		"Failed to get IMFTransform interface from H264 decoder MFT object.\n");

	IMFAttributes* decoderAttributes;
	CHECK_HR(m_pDecoderTransform->GetAttributes(&decoderAttributes),
		"Can't get attributes.");

	CHECK_HR(decoderAttributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE),
		"Failed to enable CODECAPI_AVDecVideoAcceleration_H264");

	CHECK_HR(decoderAttributes->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE),
		"Failed to enable CODECAPI_AVLowLatencyMode");

	decoderAttributes->Release();


	MFCreateMediaType(&m_pDecInputMediaType);
	m_pDecInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	m_pDecInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	CHECK_HR(MFSetAttributeSize(m_pDecInputMediaType, MF_MT_FRAME_SIZE, width, height),
		"Failed to set image size");

	CHECK_HR(MFSetAttributeRatio(m_pDecInputMediaType, MF_MT_FRAME_RATE, FRAME_RATE, 1),
		"Failed to set frame rate on H264 MFT out type.\n");

	CHECK_HR(MFSetAttributeRatio(m_pDecInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
		"Failed to set aspect ratio on H264 MFT out type.\n");

	CHECK_HR(m_pDecoderTransform->SetInputType(0, m_pDecInputMediaType, 0),
		"Failed to set input media type on H.264 decoder MFT.\n");

	MFCreateMediaType(&m_pDecOutputMediaType);
	m_pDecOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	m_pDecOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_I420);
//	m_pDecOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);

	CHECK_HR(MFSetAttributeSize(m_pDecOutputMediaType, MF_MT_FRAME_SIZE, width, height),
		"Failed to set frame size on H264 MFT out type.\n");

	CHECK_HR(MFSetAttributeRatio(m_pDecOutputMediaType, MF_MT_FRAME_RATE, FRAME_RATE, 1),
		"Failed to set frame rate on H264 MFT out type.\n");

	CHECK_HR(MFSetAttributeRatio(m_pDecOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
		"Failed to set aspect ratio on H264 MFT out type.\n");

	CHECK_HR(m_pDecoderTransform->SetOutputType(0, m_pDecOutputMediaType, 0),
		"Failed to set output media type on H.264 decoder MFT.\n");

	CHECK_HR(m_pDecoderTransform->GetInputStatus(0, &m_mftStatus),
		"Failed to get input status from H.264 decoder MFT.\n");

	if (MFT_INPUT_STATUS_ACCEPT_DATA != m_mftStatus)
	{
		printf("H.264 decoder MFT is not accepting data.\n");
		return;
	}

	CHECK_HR(m_pDecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL),
		"Failed to process FLUSH command on H.264 decoder MFT.\n");

	CHECK_HR(m_pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL),
		"Failed to process BEGIN_STREAMING command on H.264 decoder MFT.\n");

	CHECK_HR(m_pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL),
		"Failed to process START_OF_STREAM command on H.264 decoder MFT.\n");

	memset(&m_outputDataBuffer, 0, sizeof(m_outputDataBuffer));
}

void VideoDecoder::Deinitialize()
{
	if (!m_pInputBuffer)
	{
		return;
	}

	m_pFileVideoMediaType->Release();
	m_pDecTransformUnk->Release();
	m_pDecoderTransform->Release();
	m_pDecInputMediaType->Release();
	m_pDecOutputMediaType->Release();

	delete []m_pInputBuffer;
	m_pInputBuffer = nullptr;
}

int VideoDecoder::DecodeH264VideoFrame(
	const uint8_t* h264FrameData,
	const int h264FrameLength,
	int width,
	int height, 
	uint8_t** i420FrameData,
	int* i420FrameLength)
{
	// Updates the input buffer.
	memcpy(m_pInputBuffer + m_iOffset, h264FrameData, h264FrameLength);
	m_iOffset += h264FrameLength;

	// Now re-constuct.
	DWORD srcBufLength = m_iOffset;
	MFCreateSample(&m_pVideoSample);
	CHECK_HR(MFCreateMemoryBuffer(srcBufLength, &m_pReConstructedBuffer),
		"Failed to create memory buffer.\n");

	CHECK_HR(m_pVideoSample->AddBuffer(m_pReConstructedBuffer),
		"Failed to add buffer to re-constructed sample.\n");

	byte* reconByteBuffer;
	DWORD reconBuffCurrLen = 0;
	DWORD reconBuffMaxLen = 0;
	CHECK_HR(m_pReConstructedBuffer->Lock(&reconByteBuffer, &reconBuffMaxLen, &reconBuffCurrLen),
		"Error locking recon buffer.\n");

	memcpy(reconByteBuffer, m_pInputBuffer, m_iOffset);
	CHECK_HR(m_pReConstructedBuffer->Unlock(), "Error unlocking recon buffer.\n");
	m_pReConstructedBuffer->SetCurrentLength(m_iOffset);

	CHECK_HR(m_pDecoderTransform->ProcessInput(0, m_pVideoSample, 0),
		"The H264 decoder ProcessInput call failed.\n");

	CHECK_HR(m_pDecoderTransform->GetOutputStatus(&m_mftOutFlags),
		"H264 MFT GetOutputStatus failed.\n");

	CHECK_HR(m_pDecoderTransform->GetOutputStreamInfo(0, &m_streamInfo),
		"Failed to get output stream info from H264 MFT.\n");

	bool isFinished = false;
	while (true)
	{
		CHECK_HR(MFCreateSample(&m_pOutSample), "Failed to create MF sample.\n");
		CHECK_HR(MFCreateMemoryBuffer(m_streamInfo.cbSize, &m_pBuffer), "Failed to create memory buffer.\n");
		CHECK_HR(m_pOutSample->AddBuffer(m_pBuffer), "Failed to add sample to buffer.\n");
		m_outputDataBuffer.dwStreamID = 0;
		m_outputDataBuffer.dwStatus = 0;
		m_outputDataBuffer.pEvents = NULL;
		m_outputDataBuffer.pSample = m_pOutSample;

		m_mftProcessOutput = m_pDecoderTransform->ProcessOutput(
			0, 1, &m_outputDataBuffer, &m_processOutputStatus);

		if (m_mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
		{
			// ToDo: These two lines are not right.
			// Need to work out where to get timestamp and duration from the H264 decoder MFT.
			CHECK_HR(m_outputDataBuffer.pSample->SetSampleTime(m_llVideoTimeStamp),
				"Error getting YUV sample time.\n");

			CHECK_HR(m_outputDataBuffer.pSample->SetSampleDuration(m_llSampleDuration),
				"Error getting YUV sample duration.\n");

			IMFMediaBuffer* buf = NULL;
			DWORD bufLength;
			CHECK_HR(m_pOutSample->ConvertToContiguousBuffer(&buf),
				"ConvertToContiguousBuffer failed.\n");

			CHECK_HR(buf->GetCurrentLength(&bufLength),
				"Get buffer length failed.\n");

			printf("Writing sample %i, sample time %I64d, sample duration %I64d, sample size %i.\n",
				m_sampleCount, m_yuvVideoTimeStamp, m_yuvSampleDuration, bufLength);

			byte* byteBuffer;
			DWORD buffCurrLen = 0;
			DWORD buffMaxLen = 0;
			buf->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen);
			memcpy(*i420FrameData, byteBuffer, buffCurrLen);
			*i420FrameLength = buffCurrLen;
			buf->Release();

			// Resets the input buffer.
			m_iOffset = 0;
			memset(m_pInputBuffer, 0, INPUT_BUFFER_SIZE);
		}
		else
		{
			isFinished = true;
		}

		// Releases the output buffers.
		m_pOutSample->Release();
		m_pBuffer->Release();

		if (isFinished)
		{
			break;
		}
	}

	// Releases the video sample.
	m_pVideoSample->Release();
	m_pReConstructedBuffer->Release();

	printf("Finished.\n");
	return m_iOffset;
}
