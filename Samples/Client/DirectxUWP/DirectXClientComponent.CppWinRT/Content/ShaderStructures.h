#pragma once
#include <DirectXMath.h>

namespace DirectXClientComponent_CppWinRT
{
	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionTexture
	{
		DirectX::XMFLOAT3 position;		// Position
		DirectX::XMFLOAT3 textureUV;	// Texture coordinate
	};
}