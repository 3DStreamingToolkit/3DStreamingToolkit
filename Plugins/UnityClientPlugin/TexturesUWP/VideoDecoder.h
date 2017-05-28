#pragma once

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <fstream>
#include <iostream>
#include <codecapi.h>

#include "macros.h"
#include "defs.h"

namespace WebRTCDirectXClientComponent
{
	class VideoDecoder
	{
	private:
		IMFSourceResolver*				m_pSourceResolver;
		IUnknown*						m_pSource;
		IMFMediaSource*					m_pMediaFileSource;
		IMFAttributes*					m_pVideoReaderAttributes;
		IMFSourceReader*				m_pSourceReader;
		MF_OBJECT_TYPE					m_objectType;
		IMFMediaType*					m_pFileVideoMediaType;

		IUnknown*						m_pDecTransformUnk;
		IMFTransform*					m_pDecoderTransform;
		IMFMediaType*					m_pDecInputMediaType;
		IMFMediaType*					m_pDecOutputMediaType;
		DWORD							m_mftStatus;

		// Ready to start processing frames.
		MFT_OUTPUT_DATA_BUFFER			m_outputDataBuffer;
		DWORD							m_processOutputStatus;
		IMFSample*						m_pVideoSample;
		DWORD							m_streamIndex;
		DWORD							m_flags;
		DWORD							m_bufferCount;
		LONGLONG						m_llVideoTimeStamp;
		LONGLONG						m_llSampleDuration;
		LONGLONG						m_yuvVideoTimeStamp;
		LONGLONG						m_yuvSampleDuration;
		HRESULT							m_mftProcessInput;
		HRESULT							m_mftProcessOutput;
		MFT_OUTPUT_STREAM_INFO			m_streamInfo;
		IMFSample*						m_pOutSample;
		IMFMediaBuffer*					m_pBuffer;
		IMFMediaBuffer*					m_pReConstructedBuffer;
		int								m_sampleCount;
		DWORD							m_mftOutFlags;
		DWORD							m_sampleFlags;
		LONGLONG						m_reconVideoTimeStamp;
		LONGLONG						m_reconSampleDuration;
		DWORD							m_reconSampleFlags;

		IMFMediaBuffer*					m_pInBuffer;
		char*							m_pInputBuffer;
		int								m_iOffset;

	public:
		VideoDecoder();

		~VideoDecoder();

		void Initialize(int width, int height);

		void Deinitialize();

		int DecodeH264VideoFrame(
			const uint8_t* h264FrameData,
			const int h264FrameLength,
			int width,
			int height,
			uint8_t** i420FrameData,
			int* i420FrameLength);
	};
}
