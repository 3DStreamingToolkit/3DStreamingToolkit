#pragma once

#include <wrl\client.h>
#include <wrl\wrappers\corewrappers.h>
#include <d3d11_4.h>
#include <functional>

#include "plugindefs.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvEncodeAPI.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvCPUOPSys.h"

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"

namespace StreamingToolkit
{
	// The BufferRenderer class provides methods to capture frame buffer
	// from swap chain. If the frame buffer parameter is null in constructor,
	// a buffer will be created so that it can be used for rendering.
    class BufferRenderer
    {
	public:
		BufferRenderer(int width, int height, ID3D11Device* device,
			const std::function<void()>& frame_render_func,
			ID3D11Texture2D* frame_buffer = nullptr);

		~BufferRenderer();
	
		void GetDimension(int* width, int* height);
		ID3D11RenderTargetView* GetRenderTargetView();
		ID3D11Texture2D* Capture();
		void Capture(void** buffer, int* size);
		void Render();
		void Release();
		void Resize(ID3D11Texture2D* frame_buffer);
		void Resize(int width, int height);

		// Prediction timestamp
		int64_t GetPredictionTimestamp();
		void SetPredictionTimestamp(int64_t prediction_timestamp);
		void Lock();
		void Unlock();
		bool IsLocked();

		// D3D Accessors.
		ID3D11Device* GetD3DDevice() const					{ return d3d_device_.Get();  }
		ID3D11DeviceContext* GetD3DDeviceContext() const	{ return d3d_context_.Get(); }

	private:
		void UpdateStagingBuffer();

		Microsoft::WRL::Wrappers::CriticalSection buffer_lock_;
		int width_;
		int height_;
		std::function<void()> frame_render_func_;
		D3D11_TEXTURE2D_DESC staging_frame_buffer_desc_;
		Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_buffer_;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_frame_buffer_;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> d3d_render_target_view_;
		int64_t prediction_timestamp_;
		bool sending_lock_;
	};
}
