#include "pch.h"

#include <chrono>
#include <comdef.h>
#include <comutil.h>
#include <gtest\gtest.h>
#include <map>
#include <thread>
#include <Wbemidl.h>
#include <wchar.h>

#include "client_main_window.h"
#include "CppUnitTest.h"
#include "DeviceResources.h"
#include "directx_buffer_capturer.h"
#include "directx_multi_peer_conductor.h"
#include "opengl_buffer_capturer.h"
#include "server_main_window.h"
#include "third_party\libyuv\include\libyuv.h"
#include "third_party\nvpipe\nvpipe.h"
#include "webrtc.h"
#include "webrtcH264.h"
#include "win32_data_channel_handler.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "DirectXTK.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "wbemuuid.lib")

using namespace Microsoft::WRL;
using namespace DX;
using namespace StreamingToolkit;
using namespace webrtc;

// --------------------------------------------------------------
// Encoder tests
// --------------------------------------------------------------

// Tests out initializing the H264 encoder.
// This test may fail incorrectly with an incompatible nvidia GPU
TEST(EncoderTests, CanInitializeWithDefaultParameters)
{
	auto encoder = new H264EncoderImpl(cricket::VideoCodec("H264"));
	VideoCodec codecSettings;
	SetDefaultCodecSettings(&codecSettings);
	ASSERT_TRUE(encoder->InitEncode(
		&codecSettings, kNumCores, kMaxPayloadSize) == WEBRTC_VIDEO_CODEC_OK);

	// Test correct release of encoder
	ASSERT_TRUE(encoder->Release() == WEBRTC_VIDEO_CODEC_OK);
	delete encoder;
}

// --------------------------------------------------------------
// see https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#temporarily-enabling-disabled-tests
// for how to run this test (disabled by default, as this fails without nvidia gpu support)
// --------------------------------------------------------------
// Tests out retrieving the compatible NVIDIA driver version.
TEST(EncoderTests, DISABLED_HasCompatibleGPUAndDriver)
{
	HRESULT hres;

	// Step 1: --------------------------------------------------
	// Initialize COM. ------------------------------------------

	hres = CoInitializeEx(0, COINIT_MULTITHREADED);


	// Step 2: --------------------------------------------------
	// Set general COM security levels --------------------------

	hres = CoInitializeSecurity(
		NULL,
		-1,                          // COM authentication
		NULL,                        // Authentication services
		NULL,                        // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
		NULL,                        // Authentication info
		EOAC_NONE,                   // Additional capabilities 
		NULL                         // Reserved
	);

	// Step 3: ---------------------------------------------------
	// Obtain the initial locator to WMI -------------------------

	IWbemLocator *pLoc = NULL;

	hres = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *)&pLoc);

	ASSERT_FALSE(FAILED(hres));

	// Step 4: -----------------------------------------------------
	// Connect to WMI through the IWbemLocator::ConnectServer method

	IWbemServices *pSvc = NULL;

	// Connect to the root\cimv2 namespace with
	// the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		NULL,                    // User name. NULL = current user
		NULL,                    // User password. NULL = current
		0,                       // Locale. NULL indicates current
		NULL,                    // Security flags.
		0,                       // Authority (for example, Kerberos)
		0,                       // Context object 
		&pSvc                    // pointer to IWbemServices proxy
	);

	ASSERT_FALSE(FAILED(hres));

	// Step 5: --------------------------------------------------
	// Set security levels on the proxy -------------------------

	hres = CoSetProxyBlanket(
		pSvc,                        // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities 
	);

	ASSERT_FALSE(FAILED(hres));

	// Step 6: --------------------------------------------------
	// Use the IWbemServices pointer to make requests of WMI ----

	// For example, get the name of the operating system
	IEnumWbemClassObject* pEnumerator = NULL;
	hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_VideoController"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	ASSERT_FALSE(FAILED(hres));

	// Step 7: -------------------------------------------------
	// Get the data from the query in step 6 -------------------

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	VARIANT driverNumber; // Store the driver version installed
	bool NvidiaPresent = false; // Flag for Nvidia card being present

	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		VARIANT vtProp;
		// Finds the manufacturer of the card
		hr = pclsObj->Get(L"AdapterCompatibility", 0, &vtProp, 0, 0);

		// Find the nvidia card
		if (!wcscmp(vtProp.bstrVal, L"NVIDIA")) 
		{
			// Set the Nvidia card flag to true
			NvidiaPresent = true;

			hr = pclsObj->Get(L"DriverVersion", 0, &driverNumber, 0, 0);
			wchar_t *currentDriver = driverNumber.bstrVal;
			size_t len = wcslen(currentDriver);

			//Major version number of the card is found at the -7th index
			std::wstring majorVersion(currentDriver, len - 6, 1);

			//All drivers from 3.0 onwards support nvencode
			ASSERT_TRUE(std::stoi(majorVersion) > 2);
		}

		VariantClear(&vtProp);
		pclsObj->Release();
	}
	
	// Make sure that we entered the loop
	ASSERT_TRUE(NvidiaPresent);

	// Clean Up
	VariantClear(&driverNumber);

	// Using autos from here on for simplicity
	// Test to see if nvpipe library loads correctly 
	auto hGetProcIDDLL = LoadLibrary(L"Nvpipe.dll");

	auto create_nvpipe_encoder = (nvpipe_create_encoder)GetProcAddress(hGetProcIDDLL, "nvpipe_create_encoder");
	ASSERT_TRUE(create_nvpipe_encoder);
}

// --------------------------------------------------------------
// see https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#temporarily-enabling-disabled-tests
// for how to run this test (disabled by default, as this fails without nvidia gpu support)
// --------------------------------------------------------------
// Tests out hardware encoder initialization.
TEST(EncoderTests, DISABLED_HardwareEncodingIsEnabled)
{
	// Using default settings from webrtc documentation
	const nvpipe_codec codec = NVPIPE_H264_NV;
	const uint32_t width = 1280;
	const uint32_t height = 720;
	const uint64_t bitrate = width * height * 30 * 4 * 0.07;

	// Using autos from here on for simplicity
	auto hGetProcIDDLL = LoadLibrary(L"Nvpipe.dll");

	auto create_nvpipe_encoder = (nvpipe_create_encoder)GetProcAddress(hGetProcIDDLL, "nvpipe_create_encoder");
	auto destroy_nvpipe_encoder = (nvpipe_destroy)GetProcAddress(hGetProcIDDLL, "nvpipe_destroy");
	auto encode_nvpipe = (nvpipe_encode)GetProcAddress(hGetProcIDDLL, "nvpipe_encode");
	auto reconfigure_nvpipe = (nvpipe_bitrate)GetProcAddress(hGetProcIDDLL, "nvpipe_bitrate");

	// Check to ensure that each of the functions loaded correctly (DLL exists, is functional)
	ASSERT_TRUE(create_nvpipe_encoder);
	ASSERT_TRUE(destroy_nvpipe_encoder);
	ASSERT_TRUE(encode_nvpipe);
	ASSERT_TRUE(reconfigure_nvpipe);

	// Check to ensure that the encoder can be created correctly
	auto encoder = create_nvpipe_encoder(codec, bitrate, 90, NVENC_INFINITE_GOPLENGTH, 1, false);
	ASSERT_TRUE(encoder);

	// Ensure that the encoder can be destroyed correctly
	destroy_nvpipe_encoder(encoder);

	FreeLibrary((HMODULE)hGetProcIDDLL);
}

// Tests out encoding a video frame using hardware encoder.
// This test may fail incorrectly with an incompatible nvidia GPU
TEST(EncoderTests, CanEncodeCorrectly)
{
	auto h264TestImpl = new H264TestImpl();
	h264TestImpl->SetEncoderHWEnabled(true);
	rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
		h264TestImpl->input_frame_.get()->video_frame_buffer());

	size_t bufferSize = buffer->width() * buffer->height() * 4;
	size_t RowBytes = buffer->width() * 4;
	uint8_t* rgbBuffer = new uint8_t[bufferSize];

	// Convert input frame to RGB
	libyuv::I420ToARGB(buffer->GetI420()->DataY(), buffer->GetI420()->StrideY(),
		buffer->GetI420()->DataU(), buffer->GetI420()->StrideU(),
		buffer->GetI420()->DataV(), buffer->GetI420()->StrideV(),
		rgbBuffer,
		RowBytes,
		buffer->width(), buffer->height());

	// Set RGB frame 
	h264TestImpl->input_frame_.get()->set_frame_buffer(rgbBuffer);

	// Encode frame
	ASSERT_TRUE(h264TestImpl->encoder_->Encode(*h264TestImpl->input_frame_, nullptr, nullptr) == WEBRTC_VIDEO_CODEC_OK);
	EncodedImage encodedFrame;

	// Extract encoded_frame from the encoder
	ASSERT_TRUE(h264TestImpl->WaitForEncodedFrame(&encodedFrame));

	// Check if we have a complete frame with lengh > 0
	ASSERT_TRUE(encodedFrame._completeFrame);
	ASSERT_TRUE(encodedFrame._length > 0);

	// Test correct release of encoder
	ASSERT_TRUE(h264TestImpl->encoder_->Release() == WEBRTC_VIDEO_CODEC_OK);

	delete[] rgbBuffer;
	rgbBuffer = NULL;
}

// --------------------------------------------------------------
// BufferCapturer tests
// --------------------------------------------------------------

// Tests out initializing base BufferCapturer object.
TEST(BufferCapturerTests, InitializeBufferCapturer)
{
	std::shared_ptr<BufferCapturer> capturer(new BufferCapturer());
	ASSERT_TRUE(capturer.get() != nullptr);
	ASSERT_FALSE(capturer->IsRunning());
}

// Tests out initializing DirectX device resources.
TEST(BufferCapturerTests, InitializeDirectXDeviceResources)
{
	std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
	ASSERT_TRUE(deviceResources->GetD3DDevice() != nullptr);
	ASSERT_TRUE(deviceResources->GetD3DDeviceContext() != nullptr);
	ASSERT_TRUE(deviceResources->GetSwapChain() == nullptr);
	ASSERT_TRUE(deviceResources->GetBackBufferRenderTargetView() == nullptr);
}

// Tests out initializing DirectXBufferCapturer object.
TEST(BufferCapturerTests, InitializeDirectXBufferCapturer)
{
	std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
	ASSERT_TRUE(deviceResources->GetD3DDevice() != nullptr);

	std::shared_ptr<DirectXBufferCapturer> capturer(new DirectXBufferCapturer(deviceResources->GetD3DDevice()));
	ASSERT_TRUE(capturer.get() != nullptr);
}

// Tests out initializing OpenGLBufferCapturer object.
TEST(BufferCapturerTests, InitializeOpenGLBufferCapturer)
{
	std::shared_ptr<OpenGLBufferCapturer> capturer(new OpenGLBufferCapturer());
	ASSERT_TRUE(capturer.get() != nullptr);
}

// Tests out capturing video frame using DirectXBufferCapturer.
TEST(BufferCapturerTests, CaptureFrameUsingDirectXBufferCapturer)
{
	// Init DirectX device resources.
	std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());

	//
	// Prepares texture data.
	//

	// Init texture desc.
	D3D11_TEXTURE2D_DESC texDesc = { 0 };
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.Width = 1280;
	texDesc.Height = 720;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	texDesc.Usage = D3D11_USAGE_STAGING;

	// Init texture.
	ComPtr<ID3D11Texture2D> texture = { 0 };
	deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, &texture);

	// Fill texture with white color.
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(deviceResources->GetD3DDeviceContext()->Map(
		texture.Get(), 0, D3D11_MAP_WRITE, 0, &mapped)))
	{
		memset(mapped.pData, 0xFF, texDesc.Width * texDesc.Height * 4);
		deviceResources->GetD3DDeviceContext()->Unmap(texture.Get(), 0);
	}

	// Init capturer.
	std::shared_ptr<DirectXBufferCapturer> capturer(
		new DirectXBufferCapturer(deviceResources->GetD3DDevice()));

	// Forces switching to running state to test sending frame.
	capturer->running_ = true;
	capturer->SendFrame(texture.Get());

	// Verifies staging buffer.
	ASSERT_TRUE(capturer->staging_frame_buffer_.Get() != nullptr);
	ASSERT_TRUE(capturer->staging_frame_buffer_desc_.Width == 1280);
	ASSERT_TRUE(capturer->staging_frame_buffer_desc_.Height == 720);
	if (SUCCEEDED(deviceResources->GetD3DDeviceContext()->Map(
		capturer->staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
	{
		ASSERT_TRUE(*(uint8_t*)mapped.pData == 0xFF);
		deviceResources->GetD3DDeviceContext()->Unmap(
			capturer->staging_frame_buffer_.Get(), 0);
	}
}

// Tests out capturing video frame using DirectXBufferCapturer (stereo mode).
TEST(BufferCapturerTests, CaptureFrameStereoUsingDirectXBufferCapturer)
{
	// Init DirectX device resources.
	std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());

	//
	// Prepares texture data for left eye.
	//

	// Init left eye texture desc.
	D3D11_TEXTURE2D_DESC leftTexDesc = { 0 };
	leftTexDesc.ArraySize = 1;
	leftTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	leftTexDesc.Width = 1280;
	leftTexDesc.Height = 720;
	leftTexDesc.MipLevels = 1;
	leftTexDesc.SampleDesc.Count = 1;
	leftTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	leftTexDesc.Usage = D3D11_USAGE_STAGING;

	// Init left eye texture.
	ComPtr<ID3D11Texture2D> leftTexture = { 0 };
	deviceResources->GetD3DDevice()->CreateTexture2D(
		&leftTexDesc, nullptr, &leftTexture);

	// Fill left eye texture with white color.
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(deviceResources->GetD3DDeviceContext()->Map(
		leftTexture.Get(), 0, D3D11_MAP_WRITE, 0, &mapped)))
	{
		memset(mapped.pData, 0xFF, leftTexDesc.Width * leftTexDesc.Height * 4);
		deviceResources->GetD3DDeviceContext()->Unmap(leftTexture.Get(), 0);
	}

	//
	// Prepares texture data for right eye.
	//

	// Init right eye texture desc.
	D3D11_TEXTURE2D_DESC rightTexDesc = { 0 };
	rightTexDesc.ArraySize = 1;
	rightTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	rightTexDesc.Width = 1280;
	rightTexDesc.Height = 720;
	rightTexDesc.MipLevels = 1;
	rightTexDesc.SampleDesc.Count = 1;
	rightTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	rightTexDesc.Usage = D3D11_USAGE_STAGING;

	// Init right eye texture.
	ComPtr<ID3D11Texture2D> rightTexture = { 0 };
	deviceResources->GetD3DDevice()->CreateTexture2D(
		&rightTexDesc, nullptr, &rightTexture);

	// Fill right eye texture with 0xEEEEEE color.
	if (SUCCEEDED(deviceResources->GetD3DDeviceContext()->Map(
		rightTexture.Get(), 0, D3D11_MAP_WRITE, 0, &mapped)))
	{
		memset(mapped.pData, 0xEE, rightTexDesc.Width * rightTexDesc.Height * 4);
		deviceResources->GetD3DDeviceContext()->Unmap(rightTexture.Get(), 0);
	}

	// Init capturer.
	std::shared_ptr<DirectXBufferCapturer> capturer(
		new DirectXBufferCapturer(deviceResources->GetD3DDevice()));

	// Forces switching to running state to test sending frame.
	capturer->running_ = true;
	capturer->SendFrame(leftTexture.Get(), rightTexture.Get());

	// Verifies staging buffer.
	ASSERT_TRUE(capturer->staging_frame_buffer_.Get() != nullptr);
	ASSERT_TRUE(capturer->staging_frame_buffer_desc_.Width == 1280 * 2);
	ASSERT_TRUE(capturer->staging_frame_buffer_desc_.Height == 720);
	if (SUCCEEDED(deviceResources->GetD3DDeviceContext()->Map(
		capturer->staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
	{
		ASSERT_TRUE(*(uint8_t*)mapped.pData == 0xFF);
		ASSERT_TRUE(*((uint8_t*)mapped.pData + 1280 * 4) == 0xEE);
		deviceResources->GetD3DDeviceContext()->Unmap(
			capturer->staging_frame_buffer_.Get(), 0);
	}
}

// --------------------------------------------------------------
// Decoder tests
// --------------------------------------------------------------
TEST(RtpHeaderFramePredictionTest, SendReceiveFramePredictionTimestamps) 
{
	// Test implementation initializes decoder implicitly
	auto h264TestImpl = new H264TestImpl();
	h264TestImpl->SetEncoderHWEnabled(true);
	int defaultCodecWidth = 1280;
	int defaultCodecHeight= 720;

	// Generate a frame using a framegenerator class
	auto frameGen = test::FrameGenerator::CreateSquareGenerator(
		defaultCodecWidth, defaultCodecHeight);
	
	EncodedImage encodedFrame;
	VideoFrame* newFrame = frameGen->NextFrame();
	rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(newFrame->video_frame_buffer());

	size_t bufferSize = buffer->width() * buffer->height() * 4;
	size_t RowBytes = buffer->width() * 4;
	uint8_t* rgbBuffer = new uint8_t[bufferSize];

	// NvPipe does not always have the first frame in the correct format
	// If any of the first ten encoded frames are decoded correctly, we are succesful
	bool DecodedCorrectly = false;
	for (int i = 0; i < 10; i++) 
	{
		// Convert input frame to RGB
		libyuv::I420ToARGB(buffer->GetI420()->DataY(), buffer->GetI420()->StrideY(),
			buffer->GetI420()->DataU(), buffer->GetI420()->StrideU(),
			buffer->GetI420()->DataV(), buffer->GetI420()->StrideV(),
			rgbBuffer,
			RowBytes,
			buffer->width(), buffer->height());
		
		// Set RGB frame 
		newFrame->set_frame_buffer(rgbBuffer);

		// Encode frame
		h264TestImpl->encoder_->Encode(*newFrame, nullptr, nullptr);

		// Extract encoded_frame from the encoder
		h264TestImpl->WaitForEncodedFrame(&encodedFrame);

		// Set frame prediction timestamp to 1 (arbitrary value)
		encodedFrame.prediction_timestamp_ = 1;

		// Check if we have a complete frame with lengh > 0
		ASSERT_TRUE(encodedFrame._completeFrame);
		ASSERT_TRUE(encodedFrame._length > 0);

		// The first frame needs to be a keyframe for decoding
		encodedFrame._frameType = kVideoFrameKey;
		if (WEBRTC_VIDEO_CODEC_OK == h264TestImpl->decoder_->Decode(
			encodedFrame, false, nullptr)) 
		{
			DecodedCorrectly = true;
			break;
		}
		else 
		{
			encodedFrame;
			newFrame = frameGen->NextFrame();
			buffer = newFrame->video_frame_buffer();

			bufferSize = buffer->width() * buffer->height() * 4;
			RowBytes = buffer->width() * 4;
			rgbBuffer = new uint8_t[bufferSize];
		}
	}

	ASSERT_TRUE(DecodedCorrectly);

	std::unique_ptr<VideoFrame> decodedFrame;
	rtc::Optional<uint8_t> decodedQP;
	
	// Extract decoded frame from the h264 decoder
	ASSERT_TRUE(h264TestImpl->WaitForDecodedFrame(&decodedFrame, &decodedQP));

	// Ensure the decoded frame is not empty
	ASSERT_TRUE(decodedFrame != NULL);

	// Make sure frame prediction timestamp is still valid
	ASSERT_TRUE(decodedFrame->prediction_timestamp() == encodedFrame.prediction_timestamp_);

	// Make sure that the frame has the same dimensions
	// Not always true, but true for the default tests
	ASSERT_TRUE(decodedFrame->height() == buffer->height());
	ASSERT_TRUE(decodedFrame->width() == buffer->width());

	// Test correct release of decoder
	h264TestImpl->encoder_->Release();
	ASSERT_TRUE(h264TestImpl->decoder_->Release() == WEBRTC_VIDEO_CODEC_OK);

	// Cleanup.
	delete[] rgbBuffer;
	rgbBuffer = NULL;

	delete h264TestImpl;
	h264TestImpl = NULL;
}

// --------------------------------------------------------------
// End to end tests
// --------------------------------------------------------------

// Tests out connecting the spinning cube server to the signaling server.
TEST(EndToEndTests, DISABLED_ServerConnectToSignalingServer)
{
	// Constants.
	const int timeOutMs = 5000;

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();
	shared_ptr<FullServerConfig> fullServerConfig = GlobalObject<FullServerConfig>::Get();

	rtc::EnsureWinsockInit();
	rtc::Win32SocketServer w32_ss;
	rtc::Win32Thread w32_thread(&w32_ss);
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
	rtc::InitializeSSL();

	std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
	DirectXMultiPeerConductor cond(fullServerConfig, deviceResources->GetD3DDevice());

	// Connects to signaling server.
	cond.StartLogin(fullServerConfig->webrtc_config->server_uri,
		fullServerConfig->webrtc_config->port);

	// Main loop.
	MSG msg = { 0 };
	int64_t tick = GetTickCount64();
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (cond.PeerConnection().is_connected())
			{
				rtc::CleanupSSL();
				return;
			}
			else if (GetTickCount64() - tick >= timeOutMs)
			{
				rtc::CleanupSSL();
				ASSERT_TRUE(false);
			}
		}
	}
}

// Tests out connecting the DirectX Win32 client to the signaling server.
TEST(EndToEndTests, DISABLED_ClientConnectToSignalingServer)
{
	// Constants.
	const int timeOutMs = 5000;

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();
	shared_ptr<FullServerConfig> fullServerConfig = GlobalObject<FullServerConfig>::Get();

	rtc::EnsureWinsockInit();
	rtc::Win32SocketServer w32_ss;
	rtc::Win32Thread w32_thread(&w32_ss);
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
	rtc::InitializeSSL();

	ClientMainWindow wnd("", 0, 0, 0, 0, 0);
	PeerConnectionClient client;
	rtc::scoped_refptr<Conductor> cond(
		new rtc::RefCountedObject<Conductor>(&client, &wnd, nullptr));

	// Connects to signaling server.
	cond->StartLogin(fullServerConfig->webrtc_config->server_uri,
		fullServerConfig->webrtc_config->port);

	// Main loop.
	MSG msg = { 0 };
	int64_t tick = GetTickCount64();
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (client.is_connected())
			{
				rtc::CleanupSSL();
				return;
			}
			else if (GetTickCount64() - tick >= timeOutMs)
			{
				rtc::CleanupSSL();
				ASSERT_TRUE(false);
			}
		}
	}
}

// Tests out connecting the spinning cube server and DirectX Win32 client
// to the signaling server.
TEST(EndToEndTests, DISABLED_ServerClientConnectToSignalingServer)
{
	// Constants.
	const int timeOutMs = 5000;

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();
	shared_ptr<FullServerConfig> fullServerConfig = GlobalObject<FullServerConfig>::Get();

	// Init WebRTC.
	rtc::EnsureWinsockInit();
	rtc::Win32SocketServer w32_ss;
	rtc::Win32Thread w32_thread(&w32_ss);
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
	rtc::InitializeSSL();

	// Server.
	std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
	DirectXMultiPeerConductor serverCond(fullServerConfig, deviceResources->GetD3DDevice());
	serverCond.StartLogin(fullServerConfig->webrtc_config->server_uri,
		fullServerConfig->webrtc_config->port);

	// Client.
	ClientMainWindow wnd("", 0, 0, 0, 0, 0);
	PeerConnectionClient client;
	rtc::scoped_refptr<Conductor> clientCond(
		new rtc::RefCountedObject<Conductor>(&client, &wnd, nullptr));

	clientCond->StartLogin(fullServerConfig->webrtc_config->server_uri,
		fullServerConfig->webrtc_config->port);

	// Main loop.
	MSG msg = { 0 };
	int64_t tick = GetTickCount64();
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (serverCond.PeerConnection().is_connected() &&
				client.is_connected())
			{
				rtc::CleanupSSL();
				return;
			}
			else if (GetTickCount64() - tick >= timeOutMs)
			{
				rtc::CleanupSSL();
				ASSERT_TRUE(false);
			}
		}
	}
}

// Tests out streaming from spinning cube server to DirectX client.
TEST(EndToEndTests, DISABLED_SingleClientToServer)
{
	// Constants.
	const int SignalingServerTimeOut	= 5000;
	const int PeerConnectionTimeOut		= 10000;
	const int VideoFrameTimeOut			= 10000;
	const int DataChannelTimeOut		= 10000;
	const int CleanupTime				= 5000;

	const enum TestState
	{
		WAIT_FOR_SERVER = 0,
		CONNECT_TO_SIGNALING_SERVER,
		INIT_PEER_CONNECTION,
		STREAM_VIDEO,
		OPEN_DATA_CHANNEL,
		CLEANUP
	};

	const char* TestStateStrings[]
	{
		"WAIT_FOR_SERVER",
		"CONNECT_TO_SIGNALING_SERVER",
		"INIT_PEER_CONNECTION",
		"STREAM_VIDEO",
		"OPEN_DATA_CHANNEL",
		"CLEANUP"
	};

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();
	auto fullServerConfig = GlobalObject<FullServerConfig>::Get();
	auto webrtcConfig = GlobalObject<WebRTCConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();

	// Global flags.
	bool stoppingServer = false;
	bool stoppingClient = false;
	bool failed = false;
	bool receivedInput = false;

	// Starts server.
	std::shared_ptr<DirectXMultiPeerConductor> serverCond;
	std::thread serverThread([&]()
	{
		// Init WebRTC.
		rtc::EnsureWinsockInit();
		rtc::Win32SocketServer w32_ss;
		rtc::Win32Thread w32_thread(&w32_ss);
		rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

		// Init DirectX resources.
		std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
		serverCond.reset(new DirectXMultiPeerConductor(
			fullServerConfig, deviceResources->GetD3DDevice()));

		ComPtr<ID3D11Texture2D>	renderTexture;
		D3D11_TEXTURE2D_DESC texDesc = { 0 };
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.Width = fullServerConfig->server_config->server_config.width;
		texDesc.Height = fullServerConfig->server_config->server_config.height;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		deviceResources->GetD3DDevice()->CreateTexture2D(
			&texDesc, nullptr, &renderTexture);

		// Handles data channel messages.
		std::function<void(int, const string&)> dataChannelMessageHandler([&](
			int peerId,
			const std::string& message)
		{
			char type[256];
			char body[256];
			Json::Reader reader;
			Json::Value msg;
			reader.parse(message, msg, false);
			if (msg.isMember("type") && msg.isMember("body"))
			{
				std::istringstream datastream(body);
				strcpy(type, msg.get("type", "").asCString());
				strcpy(body, msg.get("body", "").asCString());
				if (strcmp(type, "end-to-end-test") == 0)
				{
					receivedInput = !strcmp(body, "1");
				}
			}
		});

		// Sets data channel message handler.
		serverCond->SetDataChannelMessageHandler(dataChannelMessageHandler);

		// Connects to signaling server.
		serverCond->StartLogin(fullServerConfig->webrtc_config->server_uri,
			fullServerConfig->webrtc_config->port);

		// Main loop.
		MSG msg = { 0 };
		int64_t tick = GetTickCount64();
		while (!stoppingServer)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				// FPS limiter.
				const int interval = 1000 / nvEncConfig->capture_fps;
				ULONGLONG timeElapsed = GetTickCount64() - tick;
				auto peers = serverCond->Peers();
				if (peers.size() && timeElapsed >= interval)
				{
					auto peer = (DirectXPeerConductor*)peers.begin()->second.get();
					tick = GetTickCount64() - timeElapsed + interval;
					peer->SendFrame(renderTexture.Get());
				}
			}
		}

		// Cleanup.
		serverCond->Close();
		tick = GetTickCount64();
		while (GetTickCount64() - tick < CleanupTime)
		{
			Sleep(1000);
		}
	});

	// Starts client.
	std::thread clientThread([&]()
	{
		// Init WebRTC.
		rtc::EnsureWinsockInit();
		rtc::Win32SocketServer w32_ss;
		rtc::Win32Thread w32_thread(&w32_ss);
		rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

		rtc::scoped_refptr<Conductor> clientCond;
		ClientMainWindow wnd("", 0, 0, 0, 0, 0);
		PeerConnectionClient client;
		clientCond = new rtc::RefCountedObject<Conductor>(
			&client, &wnd, webrtcConfig.get());

		Win32DataChannelHandler dcHandler(clientCond.get());
		wnd.SignalDataChannelMessage.connect(
			&dcHandler, &Win32DataChannelHandler::ProcessMessage);

		// Main loop.
		MSG msg = { 0 };
		int64_t tick = GetTickCount64();
		TestState testState = WAIT_FOR_SERVER;
		while (!stoppingClient && !failed)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (!wnd.PreTranslateMessage(&msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			else
			{
				switch (testState)
				{
					case WAIT_FOR_SERVER:
						if (serverCond && serverCond->PeerConnection().is_connected())
						{
							tick = GetTickCount64();
							clientCond->StartLogin(webrtcConfig->server_uri, webrtcConfig->port);
							testState = CONNECT_TO_SIGNALING_SERVER;
						}
						else if (GetTickCount64() - tick >= SignalingServerTimeOut)
						{
							failed = true;
						}

						break;

					case CONNECT_TO_SIGNALING_SERVER:
						if (client.is_connected())
						{
							tick = GetTickCount64();
							clientCond->ConnectToPeer(serverCond->PeerConnection().id());
							testState = INIT_PEER_CONNECTION;
						}
						else if (GetTickCount64() - tick >= SignalingServerTimeOut)
						{
							failed = true;
						}

						break;

					case INIT_PEER_CONNECTION:
						if (clientCond->ice_state_ == PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted)
						{
							tick = GetTickCount64();
							testState = STREAM_VIDEO;
						}
						else if (GetTickCount64() - tick >= PeerConnectionTimeOut)
						{
							failed = true;
						}

						break;

					case STREAM_VIDEO:
						if (((ClientMainWindow::ClientVideoRenderer*)wnd.remote_video_renderer_.get())->fps())
						{
							tick = GetTickCount64();
							std::string msg ="{  \"type\":\"end-to-end-test\",  \"body\":\"1\"}";
							dcHandler.data_channel_callback_->SendInputData(msg);
							testState = OPEN_DATA_CHANNEL;
						}
						else if (GetTickCount64() - tick >= VideoFrameTimeOut)
						{
							failed = true;
						}

						break;

					case OPEN_DATA_CHANNEL:
						if (receivedInput)
						{
							tick = GetTickCount64();
							stoppingServer = true;
							clientCond->Close();
							testState = CLEANUP;
						}
						else if (GetTickCount64() - tick >= DataChannelTimeOut)
						{
							failed = true;
						}

						break;

					case CLEANUP:
						if (GetTickCount64() - tick >= CleanupTime)
						{
							stoppingClient = true;
						}

						break;
				}
			}
		}

		// Checks for failure.
		if (failed)
		{
			stoppingServer = true;
			stoppingClient = true;
			std::string failMsg = "[  ERROR   ] " + std::string(TestStateStrings[testState]) + "\n";
			std::cerr << failMsg.c_str();
			ASSERT_TRUE(false);
		}
	});

	// Waits for threads to finish.
	clientThread.join();
	serverThread.join();
}

// Tests out the latency between server and client.
TEST(EndToEndTests, DISABLED_ServerToClientLatency)
{
	// Constants.
	const int SignalingServerTimeOut = 5000;
	const int PeerConnectionTimeOut = 10000;
	const int VideoFrameTimeOut = 10000;
	const int DataChannelTimeOut = 10000;
	const int LatencyTestingTime = 10000;
	const int CleanupTime = 5000;

	const enum TestState
	{
		WAIT_FOR_SERVER = 0,
		CONNECT_TO_SIGNALING_SERVER,
		INIT_PEER_CONNECTION,
		STREAM_VIDEO,
		OPEN_DATA_CHANNEL,
		LATENCY_TESTING,
		CLEANUP
	};

	const char* TestStateStrings[]
	{
		"WAIT_FOR_SERVER",
		"CONNECT_TO_SIGNALING_SERVER",
		"INIT_PEER_CONNECTION",
		"STREAM_VIDEO",
		"OPEN_DATA_CHANNEL",
		"LATENCY_TESTING",
		"CLEANUP"
	};

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();
	auto fullServerConfig = GlobalObject<FullServerConfig>::Get();
	auto webrtcConfig = GlobalObject<WebRTCConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();

	// Global flags.
	bool stoppingServer = false;
	bool stoppingClient = false;
	bool failed = false;
	bool receivedInput = false;

	// Starts server.
	std::shared_ptr<DirectXMultiPeerConductor> serverCond;
	std::thread serverThread([&]()
	{
		// Init WebRTC.
		rtc::EnsureWinsockInit();
		rtc::Win32SocketServer w32_ss;
		rtc::Win32Thread w32_thread(&w32_ss);
		rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

		// Init DirectX resources.
		std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
		serverCond.reset(new DirectXMultiPeerConductor(
			fullServerConfig, deviceResources->GetD3DDevice()));

		ComPtr<ID3D11Texture2D>	renderTexture;
		D3D11_TEXTURE2D_DESC texDesc = { 0 };
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.Width = fullServerConfig->server_config->server_config.width;
		texDesc.Height = fullServerConfig->server_config->server_config.height;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		deviceResources->GetD3DDevice()->CreateTexture2D(
			&texDesc, nullptr, &renderTexture);

		// Handles data channel messages.
		std::function<void(int, const string&)> dataChannelMessageHandler([&](
			int peerId,
			const std::string& message)
		{
			char type[256];
			char body[256];
			Json::Reader reader;
			Json::Value msg;
			reader.parse(message, msg, false);
			if (msg.isMember("type") && msg.isMember("body"))
			{
				std::istringstream datastream(body);
				strcpy(type, msg.get("type", "").asCString());
				strcpy(body, msg.get("body", "").asCString());
				if (strcmp(type, "end-to-end-test") == 0)
				{
					receivedInput = !strcmp(body, "1");
				}
			}
		});

		// Sets data channel message handler.
		serverCond->SetDataChannelMessageHandler(dataChannelMessageHandler);

		// Connects to signaling server.
		serverCond->StartLogin(fullServerConfig->webrtc_config->server_uri,
			fullServerConfig->webrtc_config->port);

		// Main loop.
		MSG msg = { 0 };
		int64_t tick = GetTickCount64();
		while (!stoppingServer)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				// FPS limiter.
				const int interval = 1000 / nvEncConfig->capture_fps;
				ULONGLONG timeElapsed = GetTickCount64() - tick;
				auto peers = serverCond->Peers();
				if (peers.size() && timeElapsed >= interval)
				{
					auto peer = (DirectXPeerConductor*)peers.begin()->second.get();
					tick = GetTickCount64() - timeElapsed + interval;
					peer->SendFrame(renderTexture.Get(), GetTickCount64());
				}
			}
		}

		// Cleanup.
		serverCond->Close();
		tick = GetTickCount64();
		while (GetTickCount64() - tick < CleanupTime)
		{
			Sleep(1000);
		}
	});

	// Starts client.
	std::thread clientThread([&]()
	{
		// Init WebRTC.
		rtc::EnsureWinsockInit();
		rtc::Win32SocketServer w32_ss;
		rtc::Win32Thread w32_thread(&w32_ss);
		rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

		rtc::scoped_refptr<Conductor> clientCond;
		ClientMainWindow wnd("", 0, 0, 0, 0, 0);
		PeerConnectionClient client;
		clientCond = new rtc::RefCountedObject<Conductor>(
			&client, &wnd, webrtcConfig.get());

		Win32DataChannelHandler dcHandler(clientCond.get());
		wnd.SignalDataChannelMessage.connect(
			&dcHandler, &Win32DataChannelHandler::ProcessMessage);

		// Main loop.
		MSG msg = { 0 };
		int64_t tick = GetTickCount64();
		TestState testState = WAIT_FOR_SERVER;
		while (!stoppingClient && !failed)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (!wnd.PreTranslateMessage(&msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			else
			{
				switch (testState)
				{
					case WAIT_FOR_SERVER:
						if (serverCond && serverCond->PeerConnection().is_connected())
						{
							tick = GetTickCount64();
							clientCond->StartLogin(webrtcConfig->server_uri, webrtcConfig->port);
							testState = CONNECT_TO_SIGNALING_SERVER;
						}
						else if (GetTickCount64() - tick >= SignalingServerTimeOut)
						{
							failed = true;
						}

						break;

					case CONNECT_TO_SIGNALING_SERVER:
						if (client.is_connected())
						{
							tick = GetTickCount64();
							clientCond->ConnectToPeer(serverCond->PeerConnection().id());
							testState = INIT_PEER_CONNECTION;
						}
						else if (GetTickCount64() - tick >= SignalingServerTimeOut)
						{
							failed = true;
						}

						break;

					case INIT_PEER_CONNECTION:
						if (clientCond->ice_state_ == PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted)
						{
							tick = GetTickCount64();
							testState = STREAM_VIDEO;
						}
						else if (GetTickCount64() - tick >= PeerConnectionTimeOut)
						{
							failed = true;
						}

						break;

					case STREAM_VIDEO:
						if (((ClientMainWindow::ClientVideoRenderer*)wnd.remote_video_renderer_.get())->fps())
						{
							tick = GetTickCount64();
							std::string msg = "{  \"type\":\"end-to-end-test\",  \"body\":\"1\"}";
							dcHandler.data_channel_callback_->SendInputData(msg);
							testState = OPEN_DATA_CHANNEL;
						}
						else if (GetTickCount64() - tick >= VideoFrameTimeOut)
						{
							failed = true;
						}

						break;

					case OPEN_DATA_CHANNEL:
						if (receivedInput)
						{
							tick = GetTickCount64();
							testState = LATENCY_TESTING;
						}
						else if (GetTickCount64() - tick >= DataChannelTimeOut)
						{
							failed = true;
						}

						break;

					case LATENCY_TESTING:
						if (GetTickCount64() - tick >= LatencyTestingTime)
						{
							std::string msg = "[ LATENCY  ] " + std::to_string(((ClientMainWindow::ClientVideoRenderer*)wnd.remote_video_renderer_.get())->latency()) + "\n";
							std::cout << msg.c_str();
							tick = GetTickCount64();
							stoppingServer = true;
							clientCond->Close();
							testState = CLEANUP;
						}

						break;

					case CLEANUP:
						if (GetTickCount64() - tick >= CleanupTime)
						{
							stoppingClient = true;
						}

						break;
				}
			}
		}

		// Checks for failure.
		if (failed)
		{
			stoppingServer = true;
			stoppingClient = true;
			std::string failMsg = "[  ERROR   ] " + std::string(TestStateStrings[testState]) + "\n";
			std::cerr << failMsg.c_str();
			ASSERT_TRUE(false);
		}
	});

	// Waits for threads to finish.
	clientThread.join();
	serverThread.join();
}

// Tests out streaming from spinning cube server to multiple DirectX clients.
TEST(EndToEndTests, DISABLED_MultiClientsToServer)
{
	// Constants.
	const int MaxClients				= 2;
	const int SignalingServerTimeOut	= 5000;
	const int PeerConnectionTimeOut		= 10000;
	const int VideoFrameTimeOut			= 10000;
	const int LatencyTestingTime		= 10000;
	const int CleanupTime				= 5000;
	const int MinFPS					= 15;
	const int MaxLatency				= 300;

	const enum TestState
	{
		WAIT_FOR_SERVER = 0,
		CONNECT_TO_SIGNALING_SERVER,
		INIT_PEER_CONNECTION,
		STREAM_VIDEO,
		LATENCY_TESTING,
		CLEANUP
	};

	const char* TestStateStrings[]
	{
		"WAIT_FOR_SERVER",
		"CONNECT_TO_SIGNALING_SERVER",
		"INIT_PEER_CONNECTION",
		"STREAM_VIDEO",
		"LATENCY_TESTING",
		"CLEANUP"
	};

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();
	auto fullServerConfig = GlobalObject<FullServerConfig>::Get();
	auto webrtcConfig = GlobalObject<WebRTCConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();

	// Global vars.
	bool stoppingServer = false;
	std::map<int, std::thread> clientThreads;
	std::map<int, TestState> clientTestState;
	std::map<int, int> clientFPS;
	std::map<int, int> clientLatency;

	// Starts server.
	std::shared_ptr<DirectXMultiPeerConductor> serverCond;
	std::thread serverThread([&]()
	{
		// Init WebRTC.
		rtc::EnsureWinsockInit();
		rtc::Win32SocketServer w32_ss;
		rtc::Win32Thread w32_thread(&w32_ss);
		rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

		// Init DirectX resources.
		std::shared_ptr<DeviceResources> deviceResources(new DeviceResources());
		serverCond.reset(new DirectXMultiPeerConductor(
			fullServerConfig, deviceResources->GetD3DDevice()));

		ComPtr<ID3D11Texture2D>	renderTexture;
		D3D11_TEXTURE2D_DESC texDesc = { 0 };
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.Width = fullServerConfig->server_config->server_config.width;
		texDesc.Height = fullServerConfig->server_config->server_config.height;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		deviceResources->GetD3DDevice()->CreateTexture2D(
			&texDesc, nullptr, &renderTexture);

		// Connects to signaling server.
		serverCond->StartLogin(fullServerConfig->webrtc_config->server_uri,
			fullServerConfig->webrtc_config->port);

		// Main loop.
		MSG msg = { 0 };
		int64_t tick = GetTickCount64();
		while (!stoppingServer)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				// FPS limiter.
				const int interval = 1000 / nvEncConfig->capture_fps;
				ULONGLONG timeElapsed = GetTickCount64() - tick;
				if (timeElapsed >= interval)
				{
					auto peers = serverCond->Peers();
					for each (auto pair in peers)
					{
						auto peer = (DirectXPeerConductor*)pair.second.get();
						tick = GetTickCount64() - timeElapsed + interval;
						peer->SendFrame(renderTexture.Get(), GetTickCount64());
					}
				}
			}
		}

		// Cleanup.
		serverCond->Close();
		tick = GetTickCount64();
		while (GetTickCount64() - tick < CleanupTime)
		{
			Sleep(1000);
		}
	});

	// Start clients.
	for (int i = 0; i < MaxClients; i++)
	{
		clientThreads[i] = std::thread([&]()
		{
			// Stores id.
			const int id = i;

			// Init WebRTC.
			rtc::EnsureWinsockInit();
			rtc::Win32SocketServer w32_ss;
			rtc::Win32Thread w32_thread(&w32_ss);
			rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

			rtc::scoped_refptr<Conductor> clientCond;
			ClientMainWindow wnd("", 0, 0, 0, 0, 0);
			PeerConnectionClient client;
			clientCond = new rtc::RefCountedObject<Conductor>(
				&client, &wnd, webrtcConfig.get());

			// Main loop.
			MSG msg = { 0 };
			int64_t tick = GetTickCount64();
			bool stoppingClient = false;
			bool failed = false;
			clientTestState[id] = WAIT_FOR_SERVER;
			clientFPS[id] = MinFPS - 1;
			clientLatency[id] = MaxLatency + 1;
			while (!stoppingClient && !failed)
			{
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (!wnd.PreTranslateMessage(&msg))
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
				else
				{
					switch (clientTestState[id])
					{
						case WAIT_FOR_SERVER:
							if (serverCond && serverCond->PeerConnection().is_connected())
							{
								tick = GetTickCount64();
								clientCond->StartLogin(webrtcConfig->server_uri, webrtcConfig->port);
								clientTestState[id] = CONNECT_TO_SIGNALING_SERVER;
							}
							else if (GetTickCount64() - tick >= SignalingServerTimeOut)
							{
								failed = true;
							}

							break;

						case CONNECT_TO_SIGNALING_SERVER:
							if (client.is_connected())
							{
								tick = GetTickCount64();
								clientCond->ConnectToPeer(serverCond->PeerConnection().id());
								clientTestState[id] = INIT_PEER_CONNECTION;
							}
							else if (GetTickCount64() - tick >= SignalingServerTimeOut)
							{
								failed = true;
							}

							break;

						case INIT_PEER_CONNECTION:
							if (clientCond->ice_state_ == PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted)
							{
								tick = GetTickCount64();
								clientTestState[id] = STREAM_VIDEO;
							}
							else if (GetTickCount64() - tick >= PeerConnectionTimeOut)
							{
								failed = true;
							}

							break;

						case STREAM_VIDEO:
							if (((ClientMainWindow::ClientVideoRenderer*)wnd.remote_video_renderer_.get())->fps())
							{
								tick = GetTickCount64();
								clientTestState[id] = LATENCY_TESTING;
							}
							else if (GetTickCount64() - tick >= VideoFrameTimeOut)
							{
								failed = true;
							}

							break;

						case LATENCY_TESTING:
							if (GetTickCount64() - tick >= LatencyTestingTime)
							{
								auto renderer = (ClientMainWindow::ClientVideoRenderer*)wnd.remote_video_renderer_.get();
								clientLatency[id] = renderer->latency();
								clientFPS[id] = renderer->fps();
								tick = GetTickCount64();
								clientCond->Close();
								clientTestState[id] = CLEANUP;
							}

							break;

						case CLEANUP:
							if (GetTickCount64() - tick >= CleanupTime)
							{
								stoppingClient = true;
							}

							break;
					}
				}
			}
		});
	}

	// Waits for client threads to finish.
	auto clientThreadsIt = clientThreads.begin();
	while (clientThreadsIt != clientThreads.end())
	{
		clientThreadsIt->second.join();
		clientThreadsIt++;
	}

	// Stops server.
	stoppingServer = true;
	serverThread.join();

	// Checks for failure.
	auto clientTestStateIt = clientTestState.begin();
	bool failed = false;
	while (clientTestStateIt != clientTestState.end())
	{
		int id = clientTestStateIt->first;
		std::string peerInfo = "[   PEER" + std::to_string(id + 1) + "  ] ";
		TestState state = clientTestStateIt->second;
		if (state != CLEANUP)
		{
			std::string msg = peerInfo + "Error: " + std::string(TestStateStrings[state]) + "\n";
			std::cerr << msg.c_str();
			failed = true;
		}
		else
		{
			int latency = clientLatency[id];
			int fps = clientFPS[id];
			std::string latencyMsg = peerInfo + "Latency: " + std::to_string(latency) + "\n";
			std::cout << latencyMsg.c_str();
			std::string fpsMsg = peerInfo + "FPS: " + std::to_string(fps) + "\n";
			std::cout << fpsMsg.c_str();
			if (fps < MinFPS || latency > MaxLatency)
			{
				std::string failMsg = "[   ERROR  ] There is some performance issue!\n";
				std::cerr << failMsg.c_str();
				failed = true;
			}
		}

		clientTestStateIt++;
	}

	ASSERT_FALSE(failed);
}
