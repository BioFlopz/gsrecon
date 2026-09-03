
struct VertexOutput
{
    float4 position : SV_Position;
    float2 localPosition : TEXCOORD0;
};

VertexOutput main(uint vertexId : SV_VertexID)
{
    static const float2 positions[6] =
    {
        float2(-0.25f, -0.25f),
        float2( 0.25f, -0.25f),
        float2( 0.25f,  0.25f),

        float2(-0.25f, -0.25f),
        float2( 0.25f,  0.25f),
        float2(-0.25f,  0.25f)
    };

    static const float2 localPositions[6] =
    {
        float2(-2.0f, -2.0f),
        float2( 2.0f, -2.0f),
        float2( 2.0f,  2.0f),

        float2(-2.0f, -2.0f),
        float2( 2.0f,  2.0f),
        float2(-2.0f,  2.0f)
    };

    VertexOutput output;

    output.position = float4(positions[vertexId], 0.0f, 1.0f);

    output.localPosition = localPositions[vertexId];

    return output;
}