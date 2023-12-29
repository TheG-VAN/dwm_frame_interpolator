const char curr_pass[] = R"(
struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
SamplerState smp : register(s0);

bool curr : register(b0);

float4 PS(VS_OUTPUT i) : SV_TARGET{
	return backBufferTex.Sample(smp, i.tex);
}
)";