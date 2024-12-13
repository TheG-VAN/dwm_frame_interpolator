const char mult_pass[] = R"(
struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D copyMultTex : register(t0);
Texture2D changeTex : register(t1);
SamplerState smp : register(s0);

float2 PS(VS_OUTPUT i) : SV_TARGET{
    /* Explanation
	 * At any frame, all we know is if the image changed or not.
     * We want to calculate the proportion of new frames to work out the relative fps vs refresh rate
     * e.g. refresh rate is 144, fps is 60. Sequence of frames would be like: Change, No Change, No Change, Change, No Change etc.
     * We want mult to be equal to 60/144
     * To do this, we get the previous value of mult and slightly increase it if a new frame happens, and decrease if no new frame.
     * The amount we change it by is determined by a coefficient. High coefficient means faster adaptation but instable (will wobble around 55-65 at a stable 60fps)
     * To get the best of both worlds, we calculate a stable and volatile mult at the same time, using the stable value most of the time.
     * When the gap between the two is large (implying a major change in fps), we shift the stable value towards the volatile one to adapt quicker.
	 */
	float2 mult = copyMultTex.Sample(smp, float2(0, 0)).xy;
	float new_mult;
	float vol_mult;
	float delta;
	if (changeTex.Sample(smp, float2(0, 0)).x != 0) {
		new_mult = mult.x + 0.001 * (1 - mult.x);
		delta = 0.03 * (1 - mult.y);
		vol_mult = mult.y + delta;
	} else {
		new_mult = mult.x - 0.001 * mult.x;
		delta = 0.03 * mult.y;
		vol_mult = mult.y - delta;
	}
	float alpha = saturate(0.1 * abs(new_mult - vol_mult) / delta);
	new_mult = lerp(new_mult, vol_mult, alpha);
	return float2(new_mult, vol_mult);
}
)";