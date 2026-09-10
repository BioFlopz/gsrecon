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

    float2 viewportSize;
    float2 padding;
};

struct GaussianPreprocessData
{
    float viewDepth;
    uint visible;
    float2 padding;

    float4 covariance0;
    float4 covariance1;

    float4 conicRadius;

    float4 clipCenter;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GaussianGpuData> gaussians;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraGpuData> camera;

[[vk::binding(2, 0)]]
StructuredBuffer<GaussianPreprocessData> preprocessData;







struct VertexOutput
{
    float4 position : SV_Position;

    float2 pixelOffset : TEXCOORD0;

    nointerpolation float4 conicOpacity : TEXCOORD1;

    nointerpolation float3 color : TEXCOORD2;
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
    const GaussianPreprocessData preprocess = preprocessData[instanceId];
    const float2 corner = corners[vertexId];


    //
    // Reference near-camera visibility test.
    //
    // Reject Gaussian centers that are behind or too close
    // to the camera before covariance projection.
    //

    if (preprocess.visible == 0u)
    {
        VertexOutput output;

        output.position = float4(2.0f, 2.0f, 0.0f, 1.0f);
        output.pixelOffset = float2(0.0f, 0.0f);
        output.conicOpacity = float4(0.0f, 0.0f, 0.0f, 0.0f);
        output.color = float3(0.0f, 0.0f, 0.0f);

        return output;
    }

    const float4 clipCenter = preprocess.clipCenter;

    const float3 conic = preprocess.conicRadius.xyz;
    const float radiusPixels = preprocess.conicRadius.w;


    //
    // Radius from the reference is measured in pixels.
    //
    // Convert pixel radius -> NDC radius.
    //

    const float2 radiusNdc = float2(2.0f * radiusPixels / camera.viewportSize.x, 2.0f * radiusPixels / camera.viewportSize.y);


    float4 clipPosition = clipCenter;

    clipPosition.xy += corner * radiusNdc * clipCenter.w;


    VertexOutput output;

    output.position = clipPosition;

    //
    // Every corner is radiusPixels away from the
    // Gaussian center in its corresponding direction.
    // Interpolation therefore gives the fragment's
    // displacement from the center in pixel units.
    //

    output.pixelOffset = corner * radiusPixels;
    output.conicOpacity = float4(conic, gaussian.opacity);
    output.color = gaussian.color;

    return output;
}