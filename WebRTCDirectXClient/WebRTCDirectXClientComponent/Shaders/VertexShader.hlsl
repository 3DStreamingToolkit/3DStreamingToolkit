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