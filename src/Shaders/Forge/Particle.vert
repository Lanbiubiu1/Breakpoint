cbuffer cbCamera : register(b0)
{
    float4x4 gView;
    float4x4 gViewProj;
};

cbuffer cbSimulation : register(b1)
{
    float4 gParticleInfo; // x = radius
};

struct VSInput
{
    float4 position : POSITION;
    float4 velocity : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float  size     : PSIZE;
    float3 worldPos : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = input.position;
    output.position = mul(gViewProj, worldPos);
    output.size = gParticleInfo.x * 200.0f;
    output.worldPos = worldPos.xyz;
    return output;
}
