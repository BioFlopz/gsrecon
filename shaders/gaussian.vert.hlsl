
struct GaussianGpuData
{
    float3 position;
    float opacity;

    float3 scale;
    float padding0;

    float4 rotation;

    float3 color;
    float padding1;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GaussianGpuData> gaussians;

struct VertexOutput
{
    float4 position      : SV_Position;
    float2 localPosition : TEXCOORD0;
    float3 color         : TEXCOORD1;
    float opacity        : TEXCOORD2;
};

VertexOutput main(uint vertexId : SV_VertexID)
{
    static const float2 corners[6] =
    {
        float2(-1.0f, -1.0f),
        float2( 1.0f, -1.0f),
        float2( 1.0f,  1.0f),

        float2(-1.0f, -1.0f),
        float2( 1.0f,  1.0f),
        float2(-1.0f,  1.0f)
    };

    const GaussianGpuData gaussian = gaussians[0];
    const float2 corner = corners[vertexId];

    VertexOutput output;

    //
    // Temporary screen-space smoke path.
    // This is NOT the final 3D Gaussian projection.
    //
    const float2 screenPosition = gaussian.position.xy + corner * gaussian.scale.xy;

    output.position = float4(screenPosition, gaussian.position.z, 1.0f);

    //
    // ±2 gives the fragment shader a useful Gaussian falloff
    // over the extent of the quad.
    //
    output.localPosition = corner * 2.0f;

    output.color = gaussian.color;
    output.opacity = gaussian.opacity;

    return output;
}