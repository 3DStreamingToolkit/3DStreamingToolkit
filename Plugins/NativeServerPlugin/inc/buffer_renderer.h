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
    class BufferRenderer
    {
	public:
		BufferRenderer(int width, int height, ID3D11Device* device,
			const std::function<void()>& frame_render_func,
			ID3D11Texture2D* frame_buffer = nullptr);

		~BufferRenderer();
	
		void GetDimension(int* width, int* height);
		ID3D11RenderTargetView* GetRenderTargetView();
		ID3D11Texture2D* Capture(int* width = nullptr, int* height = nullptr);
		void Capture(void** buffer, int* size, int* width, int* height);
		void Render();
		void Release();
		void Resize(ID3D11Texture2D* frame_buffer);
		void Resize(int width, int height);

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
	};
}
