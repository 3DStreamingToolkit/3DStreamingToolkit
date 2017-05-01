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

static void* g_TextureHandle = NULL;
static int   g_TextureWidth = 0;
static int   g_TextureHeight = 0;
static float g_Time = 0;


static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static ID3D11Device* m_Device;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
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

void* BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch)
{
	const int rowPitch = textureWidth * 4;
	// Just allocate a system memory buffer here for simplicity
	unsigned char* data = new unsigned char[rowPitch * textureHeight];
	*outRowPitch = rowPitch;
	return data;
}


void EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr)
{
	ID3D11Texture2D* d3dtex = (ID3D11Texture2D*)textureHandle;
	assert(d3dtex);

	ID3D11DeviceContext* ctx = NULL;
	m_Device->GetImmediateContext(&ctx);

	// Update texture data, and free the memory buffer
	ctx->UpdateSubresource(d3dtex, 0, NULL, dataPtr, rowPitch, 0);
	delete[](unsigned char*)dataPtr;
	dataPtr = NULL;
	ctx->Release();
}

void* textureDataPtr;
int textureRowPitch;
bool isRawFrameReady = false;
byte* yDataBuf = NULL;
byte* uDataBuf = NULL;
byte* vDataBuf = NULL;
byte* encodedBuf = NULL;
unsigned int encodedBufLen = 0;

unsigned int wRawFrame = 0;
unsigned int hRawFrame = 0;
unsigned int yStrideBuf = 0;
unsigned int uStrideBuf = 0;
unsigned int vStrideBuf = 0;

unsigned int wH264Frame = 0;
unsigned int hH264rame = 0;
byte* h264DataBuf = NULL;


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ProcessRawFrame(unsigned int w, unsigned int h, byte* yPlane, unsigned int yStride, byte* uPlane, unsigned int uStride, byte* vPlane, unsigned int vStride)
{	
	wRawFrame = w;
	hRawFrame = h;
	yStrideBuf = yStride;
	uStrideBuf = uStride;
	vStrideBuf = vStride;

	unsigned int bufSize = w * h;			// Assume YUV420 Setup
	unsigned int bufSize2 = bufSize / 4;	
	if (yDataBuf == NULL)
	{
		yDataBuf = new byte[bufSize];
		uDataBuf = new byte[bufSize2];
		vDataBuf = new byte[bufSize2];
	}

	memcpy(yDataBuf, yPlane, bufSize);
	memcpy(uDataBuf, uPlane, bufSize2);
	memcpy(vDataBuf, vPlane, bufSize2);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ProcessH264Frame(unsigned int w, unsigned int h, byte* data, unsigned int bufSize)
{
	wRawFrame = w;
	hRawFrame = h;
	encodedBufLen = bufSize;
	
	// Maximum possible size for RGBA
	unsigned int maxBufSize = w * h * 4;

	if(encodedBuf == NULL)
	{
		encodedBuf = new byte[maxBufSize];
	}
	memcpy(encodedBuf, data, encodedBufLen);
}


static void ProcessFrameData()
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

	EndModifyTexture(textureHandle, width, height, textureRowPitch, textureDataPtr);
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	ProcessFrameData();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}