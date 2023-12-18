const char curr_pass[] = R"(
struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
SamplerState smp : register(s0);

bool curr : register(b0);

float4 PS(VS_OUTPUT i) : SV_TARGET{
	if (curr) {
		float4 radius = float4(0.7577, -0.7577, 2.907, 0);
		float2 weight = float2(0.37487566, -0.12487566);
		float2 BUFFER_PIXEL_SIZE = (i.tex / i.pos.xy) / 4;
		float4 o = weight.x * backBufferTex.Sample(smp, i.tex + radius.xx * BUFFER_PIXEL_SIZE);
		o += weight.x * backBufferTex.Sample(smp, i.tex + radius.xy * BUFFER_PIXEL_SIZE);
		o += weight.x * backBufferTex.Sample(smp, i.tex + radius.yx * BUFFER_PIXEL_SIZE);
		o += weight.x * backBufferTex.Sample(smp, i.tex + radius.yy * BUFFER_PIXEL_SIZE);
		o += weight.y * backBufferTex.Sample(smp, i.tex + radius.zw * BUFFER_PIXEL_SIZE);
		o += weight.y * backBufferTex.Sample(smp, i.tex - radius.zw * BUFFER_PIXEL_SIZE);
		o += weight.y * backBufferTex.Sample(smp, i.tex + radius.wz * BUFFER_PIXEL_SIZE);
		o += weight.y * backBufferTex.Sample(smp, i.tex - radius.wz * BUFFER_PIXEL_SIZE);	
		return o;
	} else {
		return backBufferTex.Sample(smp, i.tex);
	}
}
)";