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

struct VertextShaderInput
{
	float4 position : POSITION;
	float2 textureUV : TEXCOORD0;
};

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 textureUV : TEXCOORD0;
};

PixelShaderInput main(VertextShaderInput input)
{
	PixelShaderInput output = (PixelShaderInput)0;

	output.position = input.position;
	output.textureUV = input.textureUV;
	return output;
}