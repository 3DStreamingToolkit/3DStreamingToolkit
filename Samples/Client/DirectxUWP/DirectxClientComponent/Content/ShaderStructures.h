#pragma once

namespace DirectXClientComponent
{
    // Used to send per-vertex data to the vertex shader.
    struct VertexPositionTexture
    {
		DirectX::XMFLOAT3 position;		// Position
#ifdef HOLOLENS
		DirectX::XMFLOAT3 textureUV;	// Texture coordinate
#else // HOLOLENS
		DirectX::XMFLOAT2 textureUV;	// Texture coordinate
#endif // HOLOLENS
    };
}
