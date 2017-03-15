#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

using namespace Microsoft::WRL;

struct VERTEX
{
	float x;
	float y;
	float z;
};

namespace Toolkit3DSample
{
	class CubeRenderer
	{
	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		ComPtr<ID3D11Buffer> m_vertexBuffer;
		ComPtr<ID3D11VertexShader> m_vertexShader;
		ComPtr<ID3D11PixelShader> m_pixelShader;
		ComPtr<ID3D11InputLayout> m_inputLayout;

	public:
		CubeRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void InitGraphics();
		void InitPipeline();
		void Update();
		void Render();
	};
}
