const char main_pass[] = R"(
#define Threshold 0.01
#define Threshold2 0.25

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
SamplerState smp : register(s0);
SamplerState pointSmp : register(s1);

Texture2D prevTex : register(t1);
Texture2D motionTex : register(t2);
Texture2D multTex : register(t3);

int frametime : register(b0);
bool debug : register(b0);
int fps_mult : register(b0);

float make_line(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

float PrintDigit(float2 uv, int value) {
    uv = (uv - float2(0.5, 0.58)) * float2(0.8, -1.3);
	if (value == 1) {
        return min(min(make_line(uv, float2(-0.2, 0.45), float2(0., 0.6)),
            length(float2(uv.x, max(0., abs(uv.y - .1) - .5)))),
            length(float2(max(0., abs(uv.x) - .2), uv.y + .4)));
    }
	if (value == 2) {
        float x = min(make_line(uv, float2(0.185, 0.17), float2(-.25, -.4)),
            length(float2(max(0., abs(uv.x) - .25), uv.y + .4)));
        uv.y -= .35;
        uv.x += 0.025;
        return min(x, abs(atan2(uv.x, uv.y) - 0.63) < 1.64 ? abs(length(uv) - .275) :
            length(uv + float2(.23, -.15)));
    }
	if (value == 3) {
        uv.y -= .1;
        uv.y = abs(uv.y);
        uv.y -= .25;
        return (atan2(uv.x, uv.y) > -1. ? abs(length(uv) - .25) :
            min(length(uv + float2(.211, -.134)), length(uv + float2(.0, .25))));
    }
	if (value == 4) {
        float x = min(length(float2(uv.x - .15, max(0., abs(uv.y - .1) - .5))),
            make_line(uv, float2(0.15, 0.6), float2(-.25, -.1)));
        return min(x, length(float2(max(0., abs(uv.x) - .25), uv.y + .1)));
    }
	if (value == 5) {
        float b = min(length(float2(max(0., abs(uv.x) - .25), uv.y - .6)),
            length(float2(uv.x + .25, max(0., abs(uv.y - .36) - .236))));
        uv.y += 0.1;
        uv.x += 0.05;
        float c = abs(length(float2(uv.x, max(0., abs(uv.y) - .0))) - .3);
        return min(b, abs(atan2(uv.x, uv.y) + 1.57) < .86 && uv.x < 0. ?
            length(uv + float2(.2, .224))
            : c);
    }
	if (value == 6) {
        uv.y -= .075;
        uv = -uv;
        float b = abs(length(float2(uv.x, max(0., abs(uv.y) - .275))) - .25);
        uv.y -= .175;
        float c = abs(length(float2(uv.x, max(0., abs(uv.y) - .05))) - .25);
        return min(c, cos(atan2(uv.x, uv.y + .45) + 0.65) < 0. || (uv.x > 0. && uv.y < 0.) ? b :
            length(uv + float2(0.2, 0.6)));
    }
	if (value == 7) {
        return min(length(float2(max(0., abs(uv.x) - .25), uv.y - .6)),
            make_line(uv, float2(-0.25, -0.39), float2(0.25, 0.6)));
    }
	if (value == 8) {
        float l = length(float2(max(0., abs(uv.x) - .08), uv.y - .1 + uv.x * .07));
        uv.y -= .1;
        uv.y = abs(uv.y);
        uv.y -= .245;
        return min(abs(length(uv) - .255), l);
    }
	if (value == 9) {
        uv.y -= .125;
        float b = abs(length(float2(uv.x, max(0., abs(uv.y) - .275))) - .25);
        uv.y -= .175;
        float c = abs(length(float2(uv.x, max(0., abs(uv.y) - .05))) - .25);
        return min(c, cos(atan2(uv.x, uv.y + .45) + 0.65) < 0. || (uv.x > 0. && uv.y < 0.) ? b :
            length(uv + float2(0.2, 0.6)));
    }
    uv.y -= .1;
    return abs(length(float2(uv.x, max(0., abs(uv.y) - .25))) - .25);
}

float PrintNum(float2 uv, int value) {
    int powers[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 10000000000};
	int maxDigits = ceil(log10(value + 1));
    if (value == 0) {
        maxDigits = 1;
    }
    int iu = int(floor(uv.x));
    if( iu>=0 && iu<maxDigits )
    {
        int n = (value/powers[maxDigits-iu-1]) % 10;
		float2 uv2 = uv;
        uv2.x = frac(uv.x); 
        return PrintDigit(uv2, n);
    }
	return 0;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float f_mult;
    if (fps_mult == 1) {
        f_mult = multTex.Sample(smp, float2(0,0)).x;
    } else {
        f_mult = 1.0 / fps_mult;
    }
	int fps = round(1000000000.0 / frametime * f_mult);
	float2 pos = input.tex - float2(0.9775, 0.98);
    if (fps < 100) {
        pos.x -= 0.0075;
    }
	float2 size = float2(0.0075, 0.02);
    float printed = PrintNum(pos / size, fps);
	if (printed && printed < 0.5) {
		return lerp(backBufferTex.Sample(smp, input.tex), pow(1.05 - printed, 20) * float4(0, 1, 1, 1), pow(1 - printed, 10));
	}
    if (fps < 10) {  // Disable interpolation when fps < 10 (e.g. in menus and static screens)
        return backBufferTex.Sample(smp, input.tex);
    }
    float4 motion_smp = motionTex.Sample(smp, input.tex);
	float2 m = motion_smp.xy;
    float mse = motion_smp.z;
    float2 texelsize = input.tex / input.pos.xy;
    float mmult = motion_smp.w;
    float2 motion_in_middle = motionTex.Sample(smp, input.tex + m * mmult).xy;
    float motion_diff = saturate(0.05 * (-1 + (abs(m.x) + abs(m.y)) / (abs(motion_in_middle.x) + abs(motion_in_middle.y))));
    if (debug) {
        if (mse + motion_diff >= 1) {
            return 1;
        }
	    return float4(m * 50, (m.x + m.y) * -50, 1);
    }
    return lerp(backBufferTex.Sample(smp, input.tex + m * mmult), backBufferTex.Sample(smp, input.tex), saturate(mse + motion_diff));
}
)";
