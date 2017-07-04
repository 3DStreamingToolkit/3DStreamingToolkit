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

// A constant buffer that stores projection matrices in column-major format.
cbuffer ProjectionConstantBuffer : register(b1)
{
	float4x4 projection[2];
};

struct VertexShaderInput
{
	float4	position	: POSITION;
	float4	textureUV	: TEXCOORD0;
};

// Per-vertex data passed to the geometry shader.
// Note that the render target array index will be set by the geometry shader
// using the value of viewId.
struct VertexShaderOutput
{
	float4	position	: SV_POSITION;
	float2	textureUV	: TEXCOORD0;
	uint	viewId		: TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	float4 pos = input.position;

	// Note which view this vertex has been sent to. Used for matrix lookup.
	int idx = input.textureUV.z;

	// Correct for perspective and project the vertex position onto the screen.
	pos = mul(pos, projection[idx]);
	output.position = (min16float4)pos;
	output.textureUV = input.textureUV.xy;

	// Set the view ID. The pass-through geometry shader will set the
	// render target array index to whatever value is set here.
	output.viewId = idx;

	return output;
}