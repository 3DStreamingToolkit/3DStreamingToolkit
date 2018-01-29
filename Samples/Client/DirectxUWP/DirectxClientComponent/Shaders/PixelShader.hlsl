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

//#define UNITY_UV_STARTS_AT_TOP

Texture2D diffuseTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 textureUV : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_Target
{
#ifdef UNITY_UV_STARTS_AT_TOP
	input.textureUV.y = 1 - input.textureUV.y;
#endif

	return diffuseTexture.Sample(linearSampler, input.textureUV);
}