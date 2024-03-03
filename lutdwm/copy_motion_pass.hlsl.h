const char copy_motion_pass[] = R"(
struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D motionTex : register(t0);
SamplerState smp : register(s0);

float4 PS(VS_OUTPUT i) : SV_TARGET{
	return motionTex.Sample(smp, i.tex);
}
)";