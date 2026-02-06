cbuffer UBO : register(b0)
{
    row_major float4x4 global_MVP : packoffset(c0);
    float4 global_OutputSize : packoffset(c4);
    float4 global_OriginalSize : packoffset(c5);
    float4 global_SourceSize : packoffset(c6);
};

cbuffer Push
{
    float params_hardScan : packoffset(c0);
    float params_hardPix : packoffset(c0.y);
    float params_warpX : packoffset(c0.z);
    float params_warpY : packoffset(c0.w);
    float params_maskDark : packoffset(c1);
    float params_maskLight : packoffset(c1.y);
    float params_scaleInLinearGamma : packoffset(c1.z);
    float params_shadowMask : packoffset(c1.w);
    float params_brightBoost : packoffset(c2);
    float params_hardBloomScan : packoffset(c2.y);
    float params_hardBloomPix : packoffset(c2.z);
    float params_bloomAmount : packoffset(c2.w);
    float params_shape : packoffset(c3);
};

uniform sampler2D Source;

static float2 vTexCoord;
static float4 FragColor;
static float2 FragCoord;

struct SPIRV_Cross_Input
{
    float2 vTexCoord : TEXCOORD0;
    float2 FragCoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : COLOR0;
};

float2 Warp(inout float2 pos)
{
    pos = (pos * 2.0f) - 1.0f.xx;
    pos *= float2(1.0f + ((pos.y * pos.y) * params_warpX), 1.0f + ((pos.x * pos.x) * params_warpY));
    return (pos * 0.5f) + 0.5f.xx;
}

float ToLinear1(float c)
{
    if (params_scaleInLinearGamma == 0.0f)
    {
        return c;
    }
    float _93;
    if (c <= 0.040449999272823333740234375f)
    {
        _93 = c / 12.9200000762939453125f;
    }
    else
    {
        _93 = pow((c + 0.054999999701976776123046875f) / 1.05499994754791259765625f, 2.400000095367431640625f);
    }
    return _93;
}

float3 ToLinear(float3 c)
{
    if (params_scaleInLinearGamma == 0.0f)
    {
        return c;
    }
    float param = c.x;
    float param_1 = c.y;
    float param_2 = c.z;
    return float3(ToLinear1(param), ToLinear1(param_1), ToLinear1(param_2));
}

float3 Fetch(inout float2 pos, float2 off)
{
    pos = (floor((pos * global_SourceSize.xy) + off) + 0.5f.xx) / global_SourceSize.xy;
    float3 param = tex2D(Source, pos).xyz * params_brightBoost;
    return ToLinear(param);
}

float2 Dist(inout float2 pos)
{
    pos *= global_SourceSize.xy;
    return -((pos - floor(pos)) - 0.5f.xx);
}

float Gaus(float pos, float scale)
{
    return exp2(scale * pow(abs(pos), params_shape));
}

float3 Horz3(float2 pos, float off)
{
    float2 param = pos;
    float2 param_1 = float2(-1.0f, off);
    float3 _251 = Fetch(param, param_1);
    float3 b = _251;
    float2 param_2 = pos;
    float2 param_3 = float2(0.0f, off);
    float3 _258 = Fetch(param_2, param_3);
    float3 c = _258;
    float2 param_4 = pos;
    float2 param_5 = float2(1.0f, off);
    float3 _266 = Fetch(param_4, param_5);
    float3 d = _266;
    float2 param_6 = pos;
    float2 _270 = Dist(param_6);
    float dst = _270.x;
    float scale = params_hardPix;
    float param_7 = dst - 1.0f;
    float param_8 = scale;
    float wb = Gaus(param_7, param_8);
    float param_9 = dst + 0.0f;
    float param_10 = scale;
    float wc = Gaus(param_9, param_10);
    float param_11 = dst + 1.0f;
    float param_12 = scale;
    float wd = Gaus(param_11, param_12);
    return (((b * wb) + (c * wc)) + (d * wd)) / ((wb + wc) + wd).xxx;
}

float3 Horz5(float2 pos, float off)
{
    float2 param = pos;
    float2 param_1 = float2(-2.0f, off);
    float3 _324 = Fetch(param, param_1);
    float3 a = _324;
    float2 param_2 = pos;
    float2 param_3 = float2(-1.0f, off);
    float3 _331 = Fetch(param_2, param_3);
    float3 b = _331;
    float2 param_4 = pos;
    float2 param_5 = float2(0.0f, off);
    float3 _338 = Fetch(param_4, param_5);
    float3 c = _338;
    float2 param_6 = pos;
    float2 param_7 = float2(1.0f, off);
    float3 _345 = Fetch(param_6, param_7);
    float3 d = _345;
    float2 param_8 = pos;
    float2 param_9 = float2(2.0f, off);
    float3 _353 = Fetch(param_8, param_9);
    float3 e = _353;
    float2 param_10 = pos;
    float2 _357 = Dist(param_10);
    float dst = _357.x;
    float scale = params_hardPix;
    float param_11 = dst - 2.0f;
    float param_12 = scale;
    float wa = Gaus(param_11, param_12);
    float param_13 = dst - 1.0f;
    float param_14 = scale;
    float wb = Gaus(param_13, param_14);
    float param_15 = dst + 0.0f;
    float param_16 = scale;
    float wc = Gaus(param_15, param_16);
    float param_17 = dst + 1.0f;
    float param_18 = scale;
    float wd = Gaus(param_17, param_18);
    float param_19 = dst + 2.0f;
    float param_20 = scale;
    float we = Gaus(param_19, param_20);
    return (((((a * wa) + (b * wb)) + (c * wc)) + (d * wd)) + (e * we)) / ((((wa + wb) + wc) + wd) + we).xxx;
}

float Scan(float2 pos, float off)
{
    float2 param = pos;
    float2 _585 = Dist(param);
    float dst = _585.y;
    float param_1 = dst + off;
    float param_2 = params_hardScan;
    return Gaus(param_1, param_2);
}

float3 Tri(float2 pos)
{
    float2 param = pos;
    float param_1 = -1.0f;
    float3 a = Horz3(param, param_1);
    float2 param_2 = pos;
    float param_3 = 0.0f;
    float3 b = Horz5(param_2, param_3);
    float2 param_4 = pos;
    float param_5 = 1.0f;
    float3 c = Horz3(param_4, param_5);
    float2 param_6 = pos;
    float param_7 = -1.0f;
    float wa = Scan(param_6, param_7);
    float2 param_8 = pos;
    float param_9 = 0.0f;
    float wb = Scan(param_8, param_9);
    float2 param_10 = pos;
    float param_11 = 1.0f;
    float wc = Scan(param_10, param_11);
    return ((a * wa) + (b * wb)) + (c * wc);
}

float3 Horz7(float2 pos, float off)
{
    float2 param = pos;
    float2 param_1 = float2(-3.0f, off);
    float3 _436 = Fetch(param, param_1);
    float3 a = _436;
    float2 param_2 = pos;
    float2 param_3 = float2(-2.0f, off);
    float3 _443 = Fetch(param_2, param_3);
    float3 b = _443;
    float2 param_4 = pos;
    float2 param_5 = float2(-1.0f, off);
    float3 _450 = Fetch(param_4, param_5);
    float3 c = _450;
    float2 param_6 = pos;
    float2 param_7 = float2(0.0f, off);
    float3 _457 = Fetch(param_6, param_7);
    float3 d = _457;
    float2 param_8 = pos;
    float2 param_9 = float2(1.0f, off);
    float3 _464 = Fetch(param_8, param_9);
    float3 e = _464;
    float2 param_10 = pos;
    float2 param_11 = float2(2.0f, off);
    float3 _471 = Fetch(param_10, param_11);
    float3 f = _471;
    float2 param_12 = pos;
    float2 param_13 = float2(3.0f, off);
    float3 _479 = Fetch(param_12, param_13);
    float3 g = _479;
    float2 param_14 = pos;
    float2 _483 = Dist(param_14);
    float dst = _483.x;
    float scale = params_hardBloomPix;
    float param_15 = dst - 3.0f;
    float param_16 = scale;
    float wa = Gaus(param_15, param_16);
    float param_17 = dst - 2.0f;
    float param_18 = scale;
    float wb = Gaus(param_17, param_18);
    float param_19 = dst - 1.0f;
    float param_20 = scale;
    float wc = Gaus(param_19, param_20);
    float param_21 = dst + 0.0f;
    float param_22 = scale;
    float wd = Gaus(param_21, param_22);
    float param_23 = dst + 1.0f;
    float param_24 = scale;
    float we = Gaus(param_23, param_24);
    float param_25 = dst + 2.0f;
    float param_26 = scale;
    float wf = Gaus(param_25, param_26);
    float param_27 = dst + 3.0f;
    float param_28 = scale;
    float wg = Gaus(param_27, param_28);
    return (((((((a * wa) + (b * wb)) + (c * wc)) + (d * wd)) + (e * we)) + (f * wf)) + (g * wg)) / ((((((wa + wb) + wc) + wd) + we) + wf) + wg).xxx;
}

float BloomScan(float2 pos, float off)
{
    float2 param = pos;
    float2 _601 = Dist(param);
    float dst = _601.y;
    float param_1 = dst + off;
    float param_2 = params_hardBloomScan;
    return Gaus(param_1, param_2);
}

float3 Bloom(float2 pos)
{
    float2 param = pos;
    float param_1 = -2.0f;
    float3 a = Horz5(param, param_1);
    float2 param_2 = pos;
    float param_3 = -1.0f;
    float3 b = Horz7(param_2, param_3);
    float2 param_4 = pos;
    float param_5 = 0.0f;
    float3 c = Horz7(param_4, param_5);
    float2 param_6 = pos;
    float param_7 = 1.0f;
    float3 d = Horz7(param_6, param_7);
    float2 param_8 = pos;
    float param_9 = 2.0f;
    float3 e = Horz5(param_8, param_9);
    float2 param_10 = pos;
    float param_11 = -2.0f;
    float wa = BloomScan(param_10, param_11);
    float2 param_12 = pos;
    float param_13 = -1.0f;
    float wb = BloomScan(param_12, param_13);
    float2 param_14 = pos;
    float param_15 = 0.0f;
    float wc = BloomScan(param_14, param_15);
    float2 param_16 = pos;
    float param_17 = 1.0f;
    float wd = BloomScan(param_16, param_17);
    float2 param_18 = pos;
    float param_19 = 2.0f;
    float we = BloomScan(param_18, param_19);
    return ((((a * wa) + (b * wb)) + (c * wc)) + (d * wd)) + (e * we);
}

float3 Mask(inout float2 pos)
{
    float3 mask = float3(params_maskDark, params_maskDark, params_maskDark);
    if (params_shadowMask == 1.0f)
    {
        float _line = params_maskLight;
        float odd = 0.0f;
        if (frac(pos.x * 0.16666667163372039794921875f) < 0.5f)
        {
            odd = 1.0f;
        }
        if (frac((pos.y + odd) * 0.5f) < 0.5f)
        {
            _line = params_maskDark;
        }
        pos.x = frac(pos.x * 0.3333333432674407958984375f);
        if (pos.x < 0.333000004291534423828125f)
        {
            mask.x = params_maskLight;
        }
        else
        {
            if (pos.x < 0.66600000858306884765625f)
            {
                mask.y = params_maskLight;
            }
            else
            {
                mask.z = params_maskLight;
            }
        }
        mask *= _line;
    }
    else
    {
        if (params_shadowMask == 2.0f)
        {
            pos.x = frac(pos.x * 0.3333333432674407958984375f);
            if (pos.x < 0.333000004291534423828125f)
            {
                mask.x = params_maskLight;
            }
            else
            {
                if (pos.x < 0.66600000858306884765625f)
                {
                    mask.y = params_maskLight;
                }
                else
                {
                    mask.z = params_maskLight;
                }
            }
        }
        else
        {
            if (params_shadowMask == 3.0f)
            {
                pos.x += (pos.y * 3.0f);
                pos.x = frac(pos.x * 0.16666667163372039794921875f);
                if (pos.x < 0.333000004291534423828125f)
                {
                    mask.x = params_maskLight;
                }
                else
                {
                    if (pos.x < 0.66600000858306884765625f)
                    {
                        mask.y = params_maskLight;
                    }
                    else
                    {
                        mask.z = params_maskLight;
                    }
                }
            }
            else
            {
                if (params_shadowMask == 4.0f)
                {
                    pos = floor(pos * float2(1.0f, 0.5f));
                    pos.x += (pos.y * 3.0f);
                    pos.x = frac(pos.x * 0.16666667163372039794921875f);
                    if (pos.x < 0.333000004291534423828125f)
                    {
                        mask.x = params_maskLight;
                    }
                    else
                    {
                        if (pos.x < 0.66600000858306884765625f)
                        {
                            mask.y = params_maskLight;
                        }
                        else
                        {
                            mask.z = params_maskLight;
                        }
                    }
                }
            }
        }
    }
    return mask;
}

float ToSrgb1(float c)
{
    if (params_scaleInLinearGamma == 0.0f)
    {
        return c;
    }
    float _146;
    if (c < 0.003130800090730190277099609375f)
    {
        _146 = c * 12.9200000762939453125f;
    }
    else
    {
        _146 = (1.05499994754791259765625f * pow(c, 0.416660010814666748046875f)) - 0.054999999701976776123046875f;
    }
    return _146;
}

float3 ToSrgb(float3 c)
{
    if (params_scaleInLinearGamma == 0.0f)
    {
        return c;
    }
    float param = c.x;
    float param_1 = c.y;
    float param_2 = c.z;
    return float3(ToSrgb1(param), ToSrgb1(param_1), ToSrgb1(param_2));
}

void frag_main()
{
    float2 param = vTexCoord;
    float2 _954 = Warp(param);
    float2 pos = _954;
    float2 param_1 = pos;
    float3 outColor = Tri(param_1);
    float2 param_2 = pos;
    outColor += (Bloom(param_2) * params_bloomAmount);
    if (params_shadowMask > 0.0f)
    {
        float2 param_3 = (vTexCoord / global_OutputSize.zw) * 1.00000095367431640625f;
        float3 _981 = Mask(param_3);
        outColor *= _981;
    }
    float3 param_4 = outColor;
    FragColor = float4(ToSrgb(param_4), 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoord = stage_input.vTexCoord;
    FragCoord = stage_input.FragCoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = float4(FragColor);
    return stage_output;
}
