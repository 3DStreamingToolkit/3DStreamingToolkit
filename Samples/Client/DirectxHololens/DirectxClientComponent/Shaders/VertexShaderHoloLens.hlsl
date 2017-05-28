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

// A constant buffer that stores the model transform.
cbuffer ModelConstantBuffer : register(b0)
{
	float4x4 model;
};

// A constant buffer that stores each set of view and projection matrices in column-major format.
cbuffer ViewProjectionConstantBuffer : register(b1)
{
	float4x4 viewProjection[2];
};

struct VertexShaderInput
{
	float4	position	: POSITION;
	float2	textureUV	: TEXCOORD0;
	uint    instId		: SV_InstanceID;
};

// Per-vertex data passed to the geometry shader.
// Note that the render target array index will be set by the geometry shader
// using the value of viewId.
struct VertexShaderOutput
{
	float4	position	: SV_POSITION;
	float2	textureUV	: TEXCOORD0;
	uint	viewId		: TEXCOORD1; // SV_InstanceID % 2
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	float4 pos = input.position;

	// Note which view this vertex has been sent to. Used for matrix lookup.
	// Taking the modulo of the instance ID allows geometry instancing to be used
	// along with stereo instanced drawing; in that case, two copies of each 
	// instance would be drawn, one for left and one for right.
	int idx = input.instId % 2;

	// Transform the vertex position into world space.
	pos = mul(pos, model);

	// Correct for perspective and project the vertex position onto the screen.
	pos = mul(pos, viewProjection[idx]);

	output.position = (min16float4)pos;
	output.textureUV = input.textureUV;

	// Set the instance ID. The pass-through geometry shader will set the
	// render target array index to whatever value is set here.
	output.viewId = idx;

	return output;
}