struct VSInput
{
    float2 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4 offset;
    float4 padding[15];
};

PSInput VS(VSInput input)
{
    PSInput result;

    result.position = float4(input.position, 0.0f, 1.0f) + offset;
    result.color = input.color;

    return result;
}

float4 PS(PSInput input) : SV_TARGET
{
    return input.color;
}




