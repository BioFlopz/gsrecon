struct FragmentInput
{
    float4 position : SV_Position;

    float2 pixelOffset : TEXCOORD0;

    nointerpolation float4 conicOpacity : TEXCOORD1;

    nointerpolation float3 color : TEXCOORD2;
};


float4 main(FragmentInput input) : SV_Target0
{
    const float2 d = input.pixelOffset;
    const float4 conicOpacity = input.conicOpacity;


    //
    // Reference conic evaluation:
    //
    // power =
    //     -0.5 * (A dx^2 + C dy^2)
    //     - B dx dy
    //

    const float power = -0.5f * (conicOpacity.x * d.x * d.x + conicOpacity.z * d.y * d.y) - conicOpacity.y * d.x * d.y;


    if (power > 0.0f)
    {
        discard;
    }


    const float alpha = min(0.99f, conicOpacity.w * exp(power));


    if (alpha < 1.0f / 255.0f)
    {
        discard;
    }


    return float4(input.color, alpha);
}