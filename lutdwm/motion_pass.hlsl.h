const char motion_pass[] = R"(
#define BLOCK_SIZE								3
#define BLOCK_SIZE_HALF							(BLOCK_SIZE * 0.5 - 0.5)
#define BLOCK_AREA 								(BLOCK_SIZE * BLOCK_SIZE)
#define UI_ME_MAX_ITERATIONS_PER_LEVEL			2
#define UI_ME_SAMPLES_PER_ITERATION				10  // used to be 5
#define UI_ME_PYRAMID_UPSCALE_FILTER_RADIUS		4.0
#define linearstep(_a, _b, _x) saturate((_x - _a) * rcp(_b - _a))

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

SamplerState lodSmp : register(s0);

Texture2D motionLow : register(t0);
Texture2D currTex : register(t1);
Texture2D prevTex : register(t2);

int mip_gCurr : register(b0);
int FRAME_COUNT : register(b0);

float noise(float2 co)
{
  return frac(sin(dot(co.xy ,float2(1.0,73))) * 437580.5453);
}

float min3(float3 xyz) {
	return min(min(xyz.x, xyz.y), xyz.z);
}

float4 CalcMotionLayer(VS_OUTPUT i, float4 searchStart, out float best_sim)
{	
	float2 texelsize = i.tex / i.pos.xy;
	float3 localBlock[BLOCK_AREA];
	float3 searchBlock[BLOCK_AREA];	 

	float3 moments_local = 0;
	float3 moments_local2 = 0;
	float3 moments_search = 0;
	float3 moments_search2 = 0;
	float3 moment_covariate = 0;

	i.tex -= texelsize * BLOCK_SIZE_HALF; //since we only use to sample the blocks now, offset by half a block so we can do it easier inline
	// sample each pixel in 3x3 grid
	for(uint k = 0; k < BLOCK_SIZE * BLOCK_SIZE; k++)
	{
		float2 tuv = i.tex + float2(k / BLOCK_SIZE, k % BLOCK_SIZE) * texelsize;
		float3 t_local = currTex.SampleLevel(lodSmp, tuv, mip_gCurr - 2).xyz;
		float3 t_search = prevTex.SampleLevel(lodSmp, (tuv + searchStart), mip_gCurr - 2).xyz;

		localBlock[k] = t_local; 
		searchBlock[k] = t_search;

		moments_local += t_local;
		moments_local2 += t_local * t_local;
		moments_search += t_search;
		moments_search2 += t_search * t_search;
		moment_covariate += t_local * t_search;
	}

	moments_local /= BLOCK_AREA;
	moments_local2 /= BLOCK_AREA;
	moments_search /= BLOCK_AREA;
	moments_search2 /= BLOCK_AREA;
	moment_covariate /= BLOCK_AREA;

	float3 cossim = moment_covariate * rsqrt(moments_local2 * moments_search2);
	best_sim = saturate(min3(cossim));

	float best_features = min3(abs(moments_search * moments_search - moments_search2));

	float2 bestMotion = 0;
	float2 searchCenter = searchStart;    

	float randseed = (((dot(uint2(i.pos.xy) % 5, float2(1, 5)) * 17) % 25) + 0.5) / 25.0; //prime shuffled, similar spectral properties to bayer but faster to compute and unique values within 5x5
	randseed = frac(randseed + UI_ME_MAX_ITERATIONS_PER_LEVEL * 0.6180339887498);
	float2 randdir; sincos(randseed * 6.283, randdir.x, randdir.y); //yo dawg, I heard you like golden ratios
	float2 scale = texelsize;

	[loop]
	for(int j = 0; j < UI_ME_MAX_ITERATIONS_PER_LEVEL && best_sim < 0.999999; j++)
	{
		[loop]
		for (int s = 1; s < UI_ME_SAMPLES_PER_ITERATION && best_sim < 0.999999; s++) 
		{
			randdir = mul(randdir, float2x2(-0.7373688, 0.6754903, -0.6754903, -0.7373688));//rotate by larger golden angle			
			float2 pixelOffset = randdir * scale;
			float2 samplePos = i.tex + searchCenter + pixelOffset;			 

			float3 moments_candidate = 0;		
			float3 moments_candidate2 = 0;				
			moment_covariate = 0;

			[loop]
			for(uint k = 0; k < BLOCK_SIZE * BLOCK_SIZE; k++)
			{
				float3 t = prevTex.SampleLevel(lodSmp, (samplePos + float2(k / BLOCK_SIZE, k % BLOCK_SIZE) * texelsize), mip_gCurr - 2).xyz;
				moments_candidate += t;
				moments_candidate2 += t * t;
				moment_covariate += t * localBlock[k];
			}
			moments_candidate /= BLOCK_AREA;
			moments_candidate2 /= BLOCK_AREA;
			moment_covariate /= BLOCK_AREA;

			cossim = moment_covariate * rsqrt(moments_local2 * moments_candidate2); 
			float candidate_similarity = saturate(min3(cossim.z));

			[flatten]
			if(candidate_similarity > best_sim)					
			{
				best_sim = candidate_similarity;
				bestMotion = pixelOffset;
				best_features = min3(abs(moments_candidate * moments_candidate - moments_candidate2));
			}			
		}
		searchCenter += bestMotion;
		bestMotion = 0;
		scale *= 0.5;
	}
	float4 curr_layer = float4(searchCenter, sqrt(best_features), saturate(1 - acos(best_sim) / (3.1415927 * 0.5)));  //delayed sqrt for variance -> stddev, cossim^4 for filter
	float blendfact = linearstep(curr_layer.w, 1.0, searchStart.w) * (mip_gCurr < 6);
	return lerp(curr_layer, searchStart, sqrt(blendfact));
}

float4 atrous_upscale(VS_OUTPUT i)
{	
    float2 texelsize = 2 * i.tex / i.pos.xy;  // Times 2 because it is the texel size of the lower mip level texture
	float rand = frac(mip_gCurr * 0.2114 + (FRAME_COUNT % 16) * 0.6180339887498) * 3.1415927*0.5;
	float2x2 rotm = float2x2(cos(rand), -sin(rand), sin(rand), cos(rand)) * UI_ME_PYRAMID_UPSCALE_FILTER_RADIUS;
	const float3 gauss = float3(1, 0.85, 0.65);

	float4 gbuffer_sum = 0;
	float wsum = 1e-6;

	for(int x = -1; x <= 1; x++)
	for(int y = -1; y <= 1; y++)
	{
		float2 offs = mul(float2(x, y), rotm) * texelsize;
		float2 sample_uv = i.tex + offs;

		float4 sample_gbuf = motionLow.SampleLevel(lodSmp, sample_uv, 0);

		float ws = saturate(1 - sample_gbuf.w * 4);
		float wf = saturate(1 - sample_gbuf.z * 128.0);
		float wm = dot(sample_gbuf.xy, sample_gbuf.xy) * 4;

		float weight = exp2(-(ws + wm + wf)) * gauss[abs(x)] * gauss[abs(y)];
		gbuffer_sum += sample_gbuf * weight;
		wsum += weight;		
	}

	return gbuffer_sum / wsum;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
	float4 upscaledLowerLayer = motionLow.SampleLevel(lodSmp, input.tex, 0) * 0.95;

	[branch]
    if(mip_gCurr < 6)
    	upscaledLowerLayer = atrous_upscale(input);
	
	[branch]
	if(mip_gCurr >= 2) {
		float best_sim = 0;
		float4 motion = float4(0, 0, 0, 0);
		for (float i = mip_gCurr > 4; i <= 1; i += 1) {  // was (float i = 0; i <= 2 - (mip_gCurr > 4); i += 1) for 30fps maybe? i cant remember
			float new_best_sim;
			float4 new_motion = CalcMotionLayer(input, upscaledLowerLayer * i, new_best_sim);
			if (new_best_sim > best_sim) {
				best_sim = new_best_sim;
				motion = new_motion;
			}
		}
		return motion;
		//upscaledLowerLayer = CalcMotionLayer(input, upscaledLowerLayer.xy);
	}

    return upscaledLowerLayer;
}
)";
