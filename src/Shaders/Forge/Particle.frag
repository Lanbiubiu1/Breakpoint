cbuffer cbCamera : register(b0)
{
    float4x4 gView;
    float4x4 gViewProj;
};

cbuffer cbSimulation : register(b1)
{
    float4 gParticleInfo;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float  size     : PSIZE;
    float3 worldPos : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 color = float3(0.2, 0.6, 1.0);
    float depth = saturate(input.position.z / input.position.w);
    color = lerp(color, float3(0.9, 0.4, 0.2), depth);
    return float4(color, 1.0);
}
