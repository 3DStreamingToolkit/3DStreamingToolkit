Texture2D diffuseTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 textureUV : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 diffuseColor = diffuseTexture.Sample(linearSampler, input.textureUV);

	// Fix transparent pixel issue after conversion: argb -> yuv
	//clip(diffuseColor.a == 0 ? -1 : 1);
	//clip((diffuseColor.r < 0.05 && diffuseColor.g < 0.05 && diffuseColor.b < 0.05) ? -1 : 1);

	return diffuseColor;
}