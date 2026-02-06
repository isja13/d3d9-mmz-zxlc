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

uniform float4 gl_HalfPixel;

static float4 gl_Position;
static float4 Position;
static float2 vTexCoord;
static float2 TexCoord;

struct SPIRV_Cross_Input
{
    float4 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float2 vTexCoord : TEXCOORD0;
    float4 gl_Position : POSITION;
};

void vert_main()
{
    gl_Position = mul(Position, global_MVP);
    vTexCoord = TexCoord;
    gl_Position.x = gl_Position.x - gl_HalfPixel.x * gl_Position.w;
    gl_Position.y = gl_Position.y + gl_HalfPixel.y * gl_Position.w;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    Position = stage_input.Position;
    TexCoord = stage_input.TexCoord;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vTexCoord = vTexCoord;
    return stage_output;
}
