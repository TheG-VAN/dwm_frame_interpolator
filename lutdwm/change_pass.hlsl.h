const char change_pass[] = R"(
Texture2D currTex : register(t0);
Texture2D prevTex : register(t1);
SamplerState smp : register(s0);

RWTexture2D<float> changeTex : register(u0);

[numthreads(16, 16, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID) {
    float width;
    float height;
    currTex.GetDimensions(width, height);
    float2 tex_size = float2(width, height) / 2;
    float3 currSample = currTex.SampleLevel(smp, dispatchThreadID.xy / tex_size, 0).xyz;
    float3 prevSample = prevTex.SampleLevel(smp, dispatchThreadID.xy / tex_size, 0).xyz;

    if (any(abs(currSample - prevSample) > 0.04)) {
        changeTex[uint2(0, 0)] = 1;
    }
}
)";