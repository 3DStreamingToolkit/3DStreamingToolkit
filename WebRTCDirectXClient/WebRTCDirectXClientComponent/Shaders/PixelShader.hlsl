Texture2D diffuseTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 textureUV : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_Target
{
	return diffuseTexture.Sample(linearSampler, input.textureUV);
}