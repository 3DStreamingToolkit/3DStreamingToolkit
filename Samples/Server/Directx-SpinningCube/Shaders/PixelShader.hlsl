// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	min16float4 pos         : SV_POSITION;
	min16float3 color       : COLOR0;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}
