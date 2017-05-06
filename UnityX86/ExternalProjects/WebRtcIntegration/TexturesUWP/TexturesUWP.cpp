// Required Plugin Handlers - Based on UNITY LOW LEVEL PLUGIN EXAMPLE
// https://docs.unity3d.com/Manual/NativePluginInterface.html
// https://bitbucket.org/Unity-Technologies/graphicsdemos

#include "pch.h"
#include "TexturesUWP.h"
#include <cmath>
#include <d3d11.h>
#include <assert.h>
#include "UnityPluginAPI/IUnityGraphics.h"
#include "UnityPluginAPI/IUnityInterface.h"
#include "UnityPluginAPI/IUnityGraphicsD3D11.h"
#include "VideoDecoder.h"
#include "libyuv/convert_argb.h"


// Decoder Namespace
using namespace WebRTCDirectXClientComponent;
int const textureWidth = 1280;
int const textureHeight = 720;

static void* g_TextureHandle = NULL;
static int   g_TextureWidth = textureWidth;
static int   g_TextureHeight = textureHeight;
static float g_Time = 0;

static VideoDecoder* videoDecoder = NULL;
static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static ID3D11Device* m_Device;

void* textureDataPtr;
bool isARGBFrameReady = false;

byte* yuvDataBuf = NULL;
byte* yDataBuf = NULL;
byte* uDataBuf = NULL;
byte* vDataBuf = NULL;
int yuvDataBufLen = 0;
unsigned int wRawFrame = textureWidth;
unsigned int hRawFrame = textureHeight;
unsigned int pixelSize = 4;
unsigned int yStrideBuf = 0;
unsigned int uStrideBuf = 0;
unsigned int vStrideBuf = 0;

byte* h264DataBuf = NULL;
unsigned int h264DataBufLen = 0;
unsigned int wH264Frame = textureWidth;
unsigned int hH264Frame = textureHeight;
byte* argbDataBuf = NULL;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

void InitializeVideoProcessing()
{
	// Setup Video Decoder
	videoDecoder = new VideoDecoder();
	videoDecoder->Initialize(hH264Frame, wH264Frame);

	// Pre-Allocate Buffers instead of dynamic allocation and release
	int bufSize = g_TextureWidth * g_TextureHeight * pixelSize;
	yuvDataBuf = new byte[bufSize];
	memset(yuvDataBuf, 0, bufSize);
	h264DataBuf = new byte[bufSize];
	memset(h264DataBuf, 0, bufSize);
	argbDataBuf = new byte[bufSize];
	memset(argbDataBuf, 0, bufSize);
}

void ShutdownVideoProcessing()
{
	// Release Byte Buffers
	if (yuvDataBuf != NULL)
		delete[] yuvDataBuf;
	if (h264DataBuf != NULL)
		delete[] h264DataBuf;
	if (argbDataBuf != NULL)
		delete[] argbDataBuf;

	// Cleanup Video Decoder
	if (videoDecoder != NULL)
		videoDecoder->Deinitialize();
}

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	
	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);	
	InitializeVideoProcessing();	
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
	ShutdownVideoProcessing();
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	switch (eventType)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		//TODO: user initialization code		
		s_DeviceType = s_Graphics->GetRenderer();
		IUnityGraphicsD3D11* d3d = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
		m_Device = d3d->GetDevice();
		break;
	}
	case kUnityGfxDeviceEventShutdown:
	{
		//TODO: user shutdown code
		s_DeviceType = kUnityGfxRendererNull;		
		break;
	}
	case kUnityGfxDeviceEventBeforeReset:
	{
		break;
	}
	case kUnityGfxDeviceEventAfterReset:
	{
		break;
	}
	};
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* textureHandle, int w, int h)
{
	// A script calls this at initialization time; just remember the texture pointer here.
	// Will update texture pixels each frame from the plugin rendering event (texture update
	// needs to happen on the rendering thread).
	g_TextureHandle = textureHandle;
	g_TextureWidth = w;
	g_TextureHeight = h;
}


void UpdateUnityTexture(void* textureHandle, int rowPitch, void* dataPtr)
{
	ID3D11Texture2D* d3dtex = (ID3D11Texture2D*)textureHandle;
	assert(d3dtex);

	ID3D11DeviceContext* ctx = NULL;
	m_Device->GetImmediateContext(&ctx);

	ctx->UpdateSubresource(d3dtex, 0, NULL, dataPtr, rowPitch, 0);
	ctx->Release();
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ProcessRawFrame(unsigned int w, unsigned int h, byte* yPlane, unsigned int yStride, byte* uPlane, unsigned int uStride, byte* vPlane, unsigned int vStride)
{	
	if ((yPlane == NULL) || (vPlane == NULL) || (uPlane == NULL))
		return;

	wRawFrame = w;
	hRawFrame = h;

// TODO: Remove Byte Buffers - Consume parameter data directly	
//	yStrideBuf = yStride;
//	uStrideBuf = uStride;
//	vStrideBuf = vStride;
//	unsigned int bufSize = w * h;							// Assume YUV420 Setup
//	unsigned int bufSize2 = (w + 1) / 2 * (h + 1) /2;	

//	if (yDataBuf == NULL)
//	{
//		yDataBuf = new byte[bufSize];
//		uDataBuf = new byte[bufSize2];
//		vDataBuf = new byte[bufSize2];
//	}
//
//	memcpy(yDataBuf, yPlane, bufSize);
//	memcpy(uDataBuf, uPlane, bufSize2);
//	memcpy(vDataBuf, vPlane, bufSize2);

	// TODO:  Test VP8 RawFrame Event Setup
	// Process YUV to ARGB	
	int half_width = (wRawFrame + 1) / 2;
	int half_height = (hRawFrame + 1) / 2;
	int bufSize2 = half_width * half_height;

//	yStrideBuf = wRawFrame;
//	uStrideBuf = half_width;
//	vStrideBuf = half_width;

// TODO: Remove Byte Buffers Usage
//	yDataBuf = yuvDataBuf;
//	vDataBuf = yuvDataBuf + yStrideBuf * hRawFrame;
//	uDataBuf = vDataBuf + vStrideBuf * half_height;	
//	memcpy(yDataBuf, yPlane, bufSize);	
//	memcpy(vDataBuf, vPlane, bufSize2);
//	memcpy(uDataBuf, uPlane, bufSize2);
//	isARGBFrameReady = !libyuv::I420ToARGB(
//		yDataBuf,
//		yStrideBuf,
//		uDataBuf,
//		uStrideBuf,
//		vDataBuf,
//		vStrideBuf,
//		argbDataBuf,
//		wRawFrame * pixelSize,
//		wRawFrame,
//		hRawFrame);	

	isARGBFrameReady = !libyuv::I420ToARGB(
		yPlane,
		yStride,
		uPlane,
		uStride,
		vPlane,
		vStride,
		argbDataBuf,
		wRawFrame * pixelSize,
		wRawFrame,
		hRawFrame);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ProcessH264Frame(unsigned int w, unsigned int h, byte* data, unsigned int bufSize)
{
	wRawFrame = w;
	hRawFrame = h;
	wH264Frame = w;
	hH264Frame = h;
	isARGBFrameReady = false;

	// TODO: remove h264DataBuf -- not used
	//h264DataBufLen = bufSize;
	//memcpy(h264DataBuf, data, h264DataBufLen);

	if (!videoDecoder->DecodeH264VideoFrame(
		data,
		bufSize,
		wH264Frame,
		hH264Frame,
		&yuvDataBuf,
		&yuvDataBufLen))
	{				
		
		int half_width = (wH264Frame + 1) / 2;
		int half_height = (hH264Frame + 1) / 2;
	
		yStrideBuf = wH264Frame;
		uStrideBuf = half_width;
		vStrideBuf = half_width;

		// Note: Decode Information is in YVU ordering instead YUV
		yDataBuf = yuvDataBuf;
		vDataBuf = yuvDataBuf + yStrideBuf * hH264Frame;
		uDataBuf = vDataBuf + vStrideBuf * half_height;		

		isARGBFrameReady = !libyuv::I420ToARGB(
			yDataBuf,
			yStrideBuf,
			uDataBuf,
			uStrideBuf,
			vDataBuf,
			vStrideBuf,
			argbDataBuf,
			wH264Frame * pixelSize,
			wH264Frame,
			hH264Frame);

		// YUY2 Output Processing
//		isARGBFrameReady = !libyuv::YUY2ToARGB(
//			yuvDataBuf,
//			wH264Frame * 2,
//			argbDataBuf,
//			wH264Frame * pixelSize,
//			wH264Frame,
//			hH264Frame);
	}	
}

static void ProcessARGBFrameData()
{
	if (!g_TextureHandle)
		return;

	if (!isARGBFrameReady)
		return;

	UpdateUnityTexture(g_TextureHandle, g_TextureWidth * pixelSize, argbDataBuf);
	isARGBFrameReady = false;
}

/*
static void ProcessRawFrameData()
{
	void* textureHandle = g_TextureHandle;
	int width = g_TextureWidth;
	int height = g_TextureHeight;
	if (!textureHandle)
		return;

	int textureRowPitch;
	void* textureDataPtr = BeginModifyTexture(textureHandle, width, height, &textureRowPitch);
	if (!textureDataPtr)
		return;

	const float t = g_Time * 4.0f;
	g_Time += 0.03f;
	byte val1 = 100;
	byte val2 = 50;

	// TODO: Switch Source either RAWFRAMES or ENCODED FRAME
	// TODO: DECODE H264 Frame to RAW

	// Process RAW FRAME DAYA
	byte *yDataPtr = yDataBuf;
	unsigned char* dst = (unsigned char*)textureDataPtr;	
	for (int y = 0; y < height; ++y)
	{
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x)
		{
			// TODO: Convert YUV420 to RGBA			
			byte dataPoint = *yDataPtr;
			yDataPtr++;
			ptr[0] = dataPoint;
			ptr[1] = dataPoint;
			ptr[2] = dataPoint;
			ptr[3] = 255;
			
			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += textureRowPitch;
	}

	UpdateUnityTexture(textureHandle, width * pixelSize , textureDataPtr);
}
*/

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{		
	ProcessARGBFrameData();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}