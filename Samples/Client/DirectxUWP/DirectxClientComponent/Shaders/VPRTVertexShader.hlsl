//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// A constant buffer that stores each set of view and projection matrices in column-major format.
cbuffer ViewProjectionConstantBuffer : register(b1)
{
	float4x4 viewProjection[2];
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float4	position	: POSITION;
	float4	textureUV	: TEXCOORD0;
};

// Per-vertex data passed to the pixel shader.
// Note that the render target array index is set here in the vertex shader.
struct VertexShaderOutput
{
	float4	position	: SV_POSITION;
	float2	textureUV	: TEXCOORD0;
	uint	rtvId		: SV_RenderTargetArrayIndex;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	float4 pos = input.position;

	// Note which view this vertex has been sent to. Used for matrix lookup.
	int idx = input.textureUV.z;

	// Correct for perspective and project the vertex position onto the screen.
	pos = mul(pos, viewProjection[idx]);
	output.position = (min16float4)pos;
	output.textureUV = input.textureUV.xy;

	// Set the render target array index.
	output.rtvId = idx;

	return output;
}
