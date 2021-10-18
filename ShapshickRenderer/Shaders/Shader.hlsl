struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4 position;
    float4 padding[15];
};

PSInput VS(VSInput input)
{
    PSInput result;    
    
    result.position = float4(input.position, 1.0f) + position;
        
    result.texCoord = input.texCoord;

    return result;
}

Texture2D DVD : register(t0);
SamplerState gSamPoint : register(s0);

float4 PS(PSInput input) : SV_TARGET
{
    float4 texColor = DVD.Sample(gSamPoint, input.texCoord);
    
    return texColor;
}




