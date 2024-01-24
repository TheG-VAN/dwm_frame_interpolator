const char motion_pass[] = R"(
#define BLOCK_SIZE								3
#define BLOCK_SIZE_HALF							(BLOCK_SIZE * 0.5 - 0.5)
#define BLOCK_AREA 								(BLOCK_SIZE * BLOCK_SIZE)
#define UI_ME_MAX_ITERATIONS_PER_LEVEL			2
#define UI_ME_SAMPLES_PER_ITERATION				10  // used to be 5

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

float noise(float2 co) {
  return frac(sin(dot(co.xy ,float2(1.0,73))) * 437580.5453);
}

float2 CalcMotionLayer(VS_OUTPUT i, float2 searchStart) {	
	float2 texelsize = i.tex / i.pos.xy;
	float3 localBlock[BLOCK_AREA];
	float3 mse = 0;

	i.tex -= texelsize * BLOCK_SIZE_HALF; //since we only use to sample the blocks now, offset by half a block so we can do it easier inline
	// sample each pixel in 3x3 grid
	for(uint k = 0; k < BLOCK_SIZE * BLOCK_SIZE; k++) {
		float2 tuv = i.tex + float2(k / BLOCK_SIZE, k % BLOCK_SIZE) * texelsize;
		float3 t_local = prevTex.SampleLevel(lodSmp, tuv, mip_gCurr).xyz;
		float3 t_search = currTex.SampleLevel(lodSmp, (tuv + searchStart), mip_gCurr).xyz;

		localBlock[k] = t_local; 
		mse += (t_local - t_search) * (t_local - t_search);
	}

	float min_mse = mse.x + mse.y + mse.z;
	float2 bestMotion = 0;
	float2 searchCenter = searchStart;

	float randseed = (((dot(uint2(i.pos.xy) % 5, float2(1, 5)) * 17) % 25) + 0.5) / 25.0; //prime shuffled, similar spectral properties to bayer but faster to compute and unique values within 5x5
	randseed = frac(randseed + UI_ME_MAX_ITERATIONS_PER_LEVEL * 0.6180339887498);
	float2 randdir; sincos(randseed * 6.283 / UI_ME_SAMPLES_PER_ITERATION, randdir.x, randdir.y);
	float2 scale = texelsize;
	float2 rot; sincos(6.283 / UI_ME_SAMPLES_PER_ITERATION, rot.x, rot.y);

	[loop]
	for(int j = 0; j < UI_ME_MAX_ITERATIONS_PER_LEVEL && min_mse > 0.001; j++) {
		[loop]
		for (int s = 1; s < UI_ME_SAMPLES_PER_ITERATION && min_mse > 0.001; s++) {
			randdir = mul(randdir, float2x2(rot.y, -rot.x, rot.x, rot.y));			
			float2 pixelOffset = randdir * scale;
			float2 samplePos = i.tex + searchCenter + pixelOffset;			 

			mse = 0;

			[loop]
			for(uint k = 0; k < BLOCK_SIZE * BLOCK_SIZE; k++) {
				float3 t = currTex.SampleLevel(lodSmp, (samplePos + float2(k / BLOCK_SIZE, k % BLOCK_SIZE) * texelsize), mip_gCurr).xyz;
				mse += (localBlock[k] - t) * (localBlock[k] - t);
			}

			float new_mse = mse.x + mse.y + mse.z;

			[flatten]
			if(new_mse < min_mse) {
				min_mse = new_mse;
				bestMotion = pixelOffset;
			}			
		}
		searchCenter += bestMotion;
		bestMotion = 0;
		scale *= 0.5;
	}
	return searchCenter;
}

// Not actually median for performance reasons, instead just pick the closest value to the mean
float2 median(float2 uv, float2 texelsize) {
	float2 ms[9];
	float2 mean = 0;
	int i = 0;
	for(int x = -1; x <= 1; x++)
	for(int y = -1; y <= 1; y++) {
		ms[i] = motionLow.SampleLevel(lodSmp, uv + texelsize * float2(x, y), 0).xy;
		mean += ms[i];
		i++;
	}
	mean /= 9;
	float minme = 100000;
	float2 best;
	for (i = 0; i < 9; i++) {
		float2 diff = abs(ms[i] - mean);
		if (diff.x + diff.y < minme) {
			minme = diff.x + diff.y;
			best = ms[i];
		}
	}
	return best;
}

float2 atrous_upscale(VS_OUTPUT i) {	
    float2 texelsize = i.tex / i.pos.xy;
	float3 localBlock[BLOCK_AREA];
	float3 mse = 0;

	i.tex -= texelsize * BLOCK_SIZE_HALF; //since we only use to sample the blocks now, offset by half a block so we can do it easier inline
	// sample each pixel in 3x3 grid
	for(uint k = 0; k < BLOCK_SIZE * BLOCK_SIZE; k++)
	{
		float2 tuv = i.tex + float2(k / BLOCK_SIZE, k % BLOCK_SIZE) * texelsize;
		float3 t_local = prevTex.SampleLevel(lodSmp, tuv, mip_gCurr).xyz;
		float3 t_search = currTex.SampleLevel(lodSmp, tuv, mip_gCurr).xyz;
		localBlock[k] = t_local; 
		mse += (t_local - t_search) * (t_local - t_search);
	}

	float min_mse = mse.x + mse.y + mse.z;
	if (min_mse < 0.001) {
		return 0;
	}

	float2 best_motion = 0;
	for(int x = -1; x <= 1; x++)
	for(int y = -1; y <= 1; y++) {
		float2 motion = motionLow.SampleLevel(lodSmp, i.tex + texelsize * BLOCK_SIZE_HALF + texelsize * 2 * float2(x, y), 0).xy;
		float2 samplePos = i.tex + motion;
		mse = 0;
		[loop]
		for(uint k = 0; k < BLOCK_SIZE * BLOCK_SIZE; k++) {
			float3 t = currTex.SampleLevel(lodSmp, (samplePos + float2(k / BLOCK_SIZE, k % BLOCK_SIZE) * texelsize), mip_gCurr).xyz;
			mse += (localBlock[k] - t) * (localBlock[k] - t);
		}

		float new_mse = mse.x + mse.y + mse.z;

		[flatten]
		if(new_mse < min_mse) {
			min_mse = new_mse;
			best_motion = motion;
		}
	}
	
	return best_motion;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
	float2 upscaledLowerLayer;

	[branch]
    if(mip_gCurr >= 0) {
    	upscaledLowerLayer = atrous_upscale(input);
	} else {
		upscaledLowerLayer = median(input.tex, 2 * input.tex / input.pos.xy);
	}
	
	[branch]
	if(mip_gCurr >= 1) {
		upscaledLowerLayer = CalcMotionLayer(input, upscaledLowerLayer);
	}

    return float4(upscaledLowerLayer, 0, 0);
}
)";
