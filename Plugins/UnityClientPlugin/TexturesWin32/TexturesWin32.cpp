// TexturesWin32.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
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

// Required Plugin Handlers
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
		//TODO: user Direct3D 9 code
		break;
	}
	case kUnityGfxDeviceEventAfterReset:
	{
		//TODO: user Direct3D 9 code
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


void UpdateUnityTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr)
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

static void ProcessTexture()
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

	unsigned char* dst = (unsigned char*)textureDataPtr;
	for (int y = 0; y < height; ++y)
	{
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x)
		{
			// Simple "plasma effect": several combined sine waves
			int vv = int(
				(127.0f + (127.0f * sinf(x / 7.0f + t))) +
				(127.0f + (127.0f * sinf(y / 5.0f - t))) +
				(127.0f + (127.0f * sinf((x + y) / 6.0f - t))) +
				(127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y)) / 4.0f - t)))
				) / 4;

			// Write the texture pixel
			//			ptr[0] = vv;
			//			ptr[1] = vv;
			//			ptr[2] = vv;
			//			ptr[3] = vv;

			ptr[0] = vv / 2;
			ptr[1] = vv;
			ptr[2] = vv;
			ptr[3] = 255;

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += textureRowPitch;
	}

	UpdateUnityTexture(textureHandle, width, height, textureRowPitch, textureDataPtr);
	return;
}

static void ProcessTexture2()
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

	unsigned char* dst = (unsigned char*)textureDataPtr;
	for (int y = 0; y < height; ++y)
	{
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x)
		{
			if (abs(x - y) < 50)
			{
				ptr[0] = val1;
				ptr[1] = val1;
				ptr[2] = val1;
				ptr[3] = 255;
			} else
			{
				ptr[0] = val2;
				ptr[1] = val2;
				ptr[2] = val2;
				ptr[3] = 255;
			}		

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += textureRowPitch;
	}

	UpdateUnityTexture(textureHandle, width, height, textureRowPitch, textureDataPtr);
	return;
}

void* textureDataPtr;
int textureRowPitch;
bool isARGBFrameReady = false;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ProcessRawFrame(int w, int h, byte* yPlane, int yStride, byte* uPlane, int uStride, byte* vPlane, int vStride)
{
	void* textureHandle = g_TextureHandle;
	int width = g_TextureWidth;
	int height = g_TextureHeight;
	if (!textureHandle)
		return;
	
	if (textureDataPtr == NULL)
	{
		textureDataPtr = BeginModifyTexture(textureHandle, width, height, &textureRowPitch);
	}
	if (!textureDataPtr)
		return;

	unsigned char* yData = yPlane;
	unsigned char* dst = (unsigned char*)textureDataPtr;
	for (int y = 0; y < height; ++y)
	{
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x)
		{
			ptr[0] = *yData;
			ptr[1] = *yData;
			ptr[2] = *yData;
			ptr[3] = 255;

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;

			// Iterate Through Frame
			yData++;
		}
		
		// To next image row
		dst += textureRowPitch;
	}

	isARGBFrameReady = true;
	//EndModifyTexture(textureHandle, width, height, textureRowPitch, textureDataPtr);
	return;
}

//static void ProcessRawFrameEnd()
//{
//	if (isRawFrameReady)
//	{
//		isRawFrameReady = false;
//		EndModifyTexture(g_TextureHandle, g_TextureWidth, g_TextureHeight, textureRowPitch, textureDataPtr);
//	}
//	return;
//}


static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	//ProcessTexture();
	ProcessTexture2();
	//ProcessRawFrameEnd();
}


extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}