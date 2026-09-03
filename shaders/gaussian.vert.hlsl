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

struct CameraGpuData
{
    row_major float4x4 view;
    row_major float4x4 projection;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GaussianGpuData> gaussians;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraGpuData> camera;

struct VertexOutput
{
    float4 position      : SV_Position;
    float2 localPosition : TEXCOORD0;
    float3 color         : TEXCOORD1;
    float opacity        : TEXCOORD2;
};

VertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
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

    const GaussianGpuData gaussian = gaussians[instanceId];

    const float2 corner = corners[vertexId];

    const float4 worldCenter = float4(gaussian.position, 1.0f);

    const float4 viewCenter = mul(worldCenter, camera.view);

    const float4 clipCenter = mul(viewCenter, camera.projection);

    //
    // Temporary footprint:
    // gaussian.scale.xy is still interpreted as NDC radius.
    //
    float4 clipPosition = clipCenter;

    clipPosition.xy += corner * gaussian.scale.xy * clipCenter.w;

    VertexOutput output;

    output.position = clipPosition;

    output.localPosition = corner * 2.0f;

    output.color = gaussian.color;

    output.opacity = gaussian.opacity;

    return output;
}