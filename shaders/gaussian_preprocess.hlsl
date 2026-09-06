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
};

struct PreprocessPushConstants
{
    uint gaussianCount;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GaussianGpuData> gaussians;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraGpuData> camera;

[[vk::binding(2, 0)]]
RWStructuredBuffer<GaussianPreprocessData> preprocessData;

[[vk::push_constant]]
PreprocessPushConstants pushConstants;

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;

    if (index >= pushConstants.gaussianCount)
    {
        return;
    }

    const float4 worldCenter = float4(gaussians[index].position, 1.0f);

    const float4 viewCenter = mul(worldCenter, camera.view);

    GaussianPreprocessData output;

    output.viewDepth = viewCenter.z;
    output.visible = viewCenter.z > 0.2f ? 1u : 0u;
    output.padding = float2(0.0f, 0.0f);

    preprocessData[index] = output;
}