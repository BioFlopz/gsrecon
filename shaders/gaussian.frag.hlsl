struct FragmentInput
{
    float4 position : SV_Position;
    float2 localPosition : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    const float distanceSquared = dot(input.localPosition, input.localPosition);

    const float power = -0.5f * distanceSquared;

    const float alpha = exp(power);

    return float4(1.0f, 0.0f, 0.0f, alpha);
}