struct Covariance3D
{
    float xx;
    float xy;
    float xz;
    float yy;
    float yz;
    float zz;
};

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



Covariance3D computeGaussianCovariance3D(GaussianGpuData gaussian)
{
    //
    // GaussianGpuData quaternion order:
    //
    //     w, x, y, z
    //
    // Same convention as the already-proven CUDA implementation.
    //

    const float w = gaussian.rotation.x;
    const float x = gaussian.rotation.y;
    const float y = gaussian.rotation.z;
    const float z = gaussian.rotation.w;

    const float r00 = 1.0f - 2.0f * (y * y + z * z);
    const float r01 = 2.0f * (x * y - w * z);
    const float r02 = 2.0f * (x * z + w * y);
    const float r10 = 2.0f * (x * y + w * z);
    const float r11 = 1.0f - 2.0f * (x * x + z * z);
    const float r12 = 2.0f * (y * z - w * x);
    const float r20 = 2.0f * (x * z - w * y);
    const float r21 = 2.0f * (y * z + w * x);
    const float r22 = 1.0f - 2.0f * (x * x + y * y);

    const float sx2 = gaussian.scale.x * gaussian.scale.x;
    const float sy2 = gaussian.scale.y * gaussian.scale.y;
    const float sz2 = gaussian.scale.z * gaussian.scale.z;

    Covariance3D covariance;

    covariance.xx =
        r00 * r00 * sx2 +
        r01 * r01 * sy2 +
        r02 * r02 * sz2;

    covariance.xy =
        r00 * r10 * sx2 +
        r01 * r11 * sy2 +
        r02 * r12 * sz2;

    covariance.xz =
        r00 * r20 * sx2 +
        r01 * r21 * sy2 +
        r02 * r22 * sz2;

    covariance.yy =
        r10 * r10 * sx2 +
        r11 * r11 * sy2 +
        r12 * r12 * sz2;

    covariance.yz =
        r10 * r20 * sx2 +
        r11 * r21 * sy2 +
        r12 * r22 * sz2;

    covariance.zz =
        r20 * r20 * sx2 +
        r21 * r21 * sy2 +
        r22 * r22 * sz2;

    return covariance;
}


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

    const Covariance3D covariance = computeGaussianCovariance3D(gaussians[index]);

    output.covariance0 = float4(covariance.xx, covariance.xy, covariance.xz, covariance.yy);
    output.covariance1 = float4(covariance.yz, covariance.zz, 0.0f, 0.0f);

    preprocessData[index] = output;
}