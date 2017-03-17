#pragma once

#include <d3d11.h>

namespace Toolkit3DLibrary
{
    class VideoHelper
    {
	public:
		VideoHelper(ID3D11Device*, ID3D11DeviceContext*);
		~VideoHelper();
		void Initialize(IDXGISwapChain*);
		void StartCapture();
		void Capture();
		void EndCapture();

	private:
		ID3D11Device* m_d3dDevice;
		ID3D11DeviceContext* m_d3dContext;
		IDXGISwapChain* m_swapChain;
    };
}
