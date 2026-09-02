

struct GaussianGpuData
{
    float3 position;
    float density;

    float3 scale;
    float padding0;

    float4 rotation;

    float3 color;
    float padding1;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<GaussianGpuData> gaussians;

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    gaussians[dispatchThreadId.x].density += 1.0f;
}