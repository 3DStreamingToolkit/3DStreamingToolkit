#pragma once

#include "DeviceResources.h"

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
	public:
		CubeRenderer(DX::DeviceResources* deviceResources);
		void InitGraphics();
		void InitPipeline();
		void Update();
		void Render();

	private:
		// Cached pointer to device resources.
		DX::DeviceResources* m_deviceResources;

		// Direct3D resources for cube geometry.
		ID3D11Buffer* m_vertexBuffer;
		ID3D11VertexShader* m_vertexShader;
		ID3D11PixelShader* m_pixelShader;
		ID3D11InputLayout* m_inputLayout;
	};
}
