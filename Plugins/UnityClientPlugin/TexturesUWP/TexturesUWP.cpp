
#include "pch.h"
#include "TexturesUWP.h"
#include <cmath>
#include <d3d11.h>
#include <assert.h>
#include "IUnityGraphics.h"
#include "IUnityInterface.h"
#include "IUnityGraphicsD3D11.h"
#include "VideoDecoder.h"
#include "libyuv/convert_argb.h"
#include <thread>


// Plugin Mode
int PluginMode = 0;

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

void ProcessTestFrameDataThread()
{
	while (true)
	{
		if (!g_TextureHandle)
		{
			Sleep(500);
			continue;;
		}

		//int bufSize = g_TextureWidth * g_TextureHeight * pixelSize;
		const float t = g_Time * 4.0f;
		g_Time += 0.03f;

		// TODO: Setup Mutex/Lock for isARGBFrameReady/argbDataBuf
		// Quick solution to coordinate render status
		isARGBFrameReady = false;

		byte* dst = argbDataBuf;
		for (int y = 0; y < g_TextureHeight; ++y)
		{
			byte* ptr = dst;
			for (int x = 0; x < g_TextureWidth; ++x)
			{
				// Simple "plasma effect": several combined sine waves
				int vv = int(
					(127.0f + (127.0f * sinf(x / 7.0f + t))) +
					(127.0f + (127.0f * sinf(y / 5.0f - t))) +
					(127.0f + (127.0f * sinf((x + y) / 6.0f - t))) +
					(127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y)) / 4.0f - t)))
					) / 4;

				ptr[0] = vv / 2;
				ptr[1] = vv;
				ptr[2] = vv;
				ptr[3] = 255;

				// To next pixel (our pixels are 4 bpp)
				ptr += 4;
			}

			// To next image row
			dst += pixelSize * g_TextureWidth;
		}

		isARGBFrameReady = true;
		Sleep(15);
	}

	// Texture Update is executed when Unity triggers the render event to avoid collision
	// between the plugin and Unity's graphics update.  Data can be manipulated but graphics
	// needs to follow Unity render event.	
	//
	//UpdateUnityTexture(g_TextureHandle, g_TextureWidth * pixelSize, argbDataBuf);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetPluginMode(int mode)
{
	PluginMode = mode;

	if(PluginMode == 2)
	{		
		// Starts the display update thread and detach to run continuously
		std::thread t(ProcessTestFrameDataThread);
		t.detach();
	}
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


	// TODO:  Test VP8 RawFrame Event Setup
	// Process YUV to ARGB	
	int half_width = (wRawFrame + 1) / 2;
	int half_height = (hRawFrame + 1) / 2;
	int bufSize2 = half_width * half_height;

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

static void ProcessTestFrameData()
{
	if (!g_TextureHandle)
		return;

	//int bufSize = g_TextureWidth * g_TextureHeight * pixelSize;
	const float t = g_Time * 4.0f;
	g_Time += 0.03f;

	byte* dst = argbDataBuf;
	for (int y = 0; y < g_TextureHeight; ++y)
	{
		byte* ptr = dst;
		for (int x = 0; x < g_TextureWidth; ++x)
		{
			// Simple "plasma effect": several combined sine waves
			int vv = int(
				(127.0f + (127.0f * sinf(x / 7.0f + t))) +
				(127.0f + (127.0f * sinf(y / 5.0f - t))) +
				(127.0f + (127.0f * sinf((x + y) / 6.0f - t))) +
				(127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y)) / 4.0f - t)))
				) / 4;

			ptr[0] = vv / 2;
			ptr[1] = vv;
			ptr[2] = vv;
			ptr[3] = 255;

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += pixelSize * g_TextureWidth;
	}

	UpdateUnityTexture(g_TextureHandle, g_TextureWidth * pixelSize, argbDataBuf);
}




static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	switch (PluginMode)
	{
	case 0:		// WebRTC Image Frame Rendering
		ProcessARGBFrameData();
		break;
	case 1:		// Invoke
		ProcessTestFrameData();
		break;
	case 2:		// Invoke
		if(isARGBFrameReady)
		{
			UpdateUnityTexture(g_TextureHandle, g_TextureWidth * pixelSize, argbDataBuf);
			isARGBFrameReady = false;
		}
		break;

	}

}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}