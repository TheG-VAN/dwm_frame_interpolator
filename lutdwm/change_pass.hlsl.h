const char change_pass[] = R"(
struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D currTex : register(t0);
Texture2D prevTex : register(t1);
SamplerState smp : register(s0);

float PS(VS_OUTPUT i) : SV_TARGET{
	return any(abs(currTex.SampleLevel(smp, i.tex, 4) - prevTex.SampleLevel(smp, i.tex, 4)) > 0.04);
}
)";