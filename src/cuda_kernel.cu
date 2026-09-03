#include <cuda_runtime.h>

#include "gaussian.hpp"

constexpr unsigned int kGaussianSmokeCount = 3;

__global__ void cudaKernelSmoke(int* value)
{
    if (threadIdx.x == 0)
    {
        *value = 1;
    }
}

__global__ void cudaExternalMemorySmoke(unsigned int* values)
{
    const unsigned int i = threadIdx.x;

    if (i < 4)
    {
        values[i] = i + 10;
    }
}

__global__ void cudaGaussianSmoke(GaussianGpuData* gaussians)
{
    const unsigned int index = threadIdx.x;

    if (index >= kGaussianSmokeCount)
    {
        return;
    }

    GaussianGpuData& gaussian = gaussians[index];

    gaussian.position[0] = index == 0 ? 0.0f : index == 1 ? -0.5f : 0.5f;

    gaussian.position[1] = 0.0f;
    gaussian.position[2] = 0.0f;

    gaussian.opacity = 1.0f;

    gaussian.scale[0] = 0.25f;
    gaussian.scale[1] = 0.25f;
    gaussian.scale[2] = 0.25f;
    gaussian.padding0 = 0.0f;

    gaussian.rotation[0] = 1.0f;
    gaussian.rotation[1] = 0.0f;
    gaussian.rotation[2] = 0.0f;
    gaussian.rotation[3] = 0.0f;

    gaussian.color[0] = index == 0 ? 1.0f : 0.0f;
    gaussian.color[1] = index == 1 ? 1.0f : 0.0f;
    gaussian.color[2] = index == 2 ? 1.0f : 0.0f;
    gaussian.padding1 = 0.0f;
}

extern "C" bool runCudaKernelSmoke()
{
    int* deviceValue = nullptr;

    if (cudaMalloc(&deviceValue, sizeof(int)) != cudaSuccess)
    {
        return false;
    }

    cudaKernelSmoke<<<1, 1>>>(deviceValue);

    if (cudaGetLastError() != cudaSuccess)
    {
        cudaFree(deviceValue);
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        cudaFree(deviceValue);
        return false;
    }

    int hostValue = 0;

    const cudaError_t copyResult = cudaMemcpy(&hostValue, deviceValue, sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(deviceValue);

    return copyResult == cudaSuccess && hostValue == 1;
}


extern "C" bool runCudaExternalMemorySmoke(void* mappedStorage)
{
    auto* values = static_cast<unsigned int*>(mappedStorage);

    cudaExternalMemorySmoke<<<1, 4>>>(values);

    if (cudaGetLastError() != cudaSuccess)
    {
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        return false;
    }

    unsigned int hostValues[4]{};

    if (cudaMemcpy(hostValues, values, sizeof(hostValues), cudaMemcpyDeviceToHost) != cudaSuccess)
    {
        return false;
    }

    return
        hostValues[0] == 10 &&
        hostValues[1] == 11 &&
        hostValues[2] == 12 &&
        hostValues[3] == 13;
}

extern "C" bool runCudaExternalGaussianWriteAsync(void* mappedStorage)
{
    auto* gaussians = static_cast<GaussianGpuData*>(mappedStorage);

    cudaGaussianSmoke<<<1, kGaussianSmokeCount>>>(gaussians);

    return cudaGetLastError() == cudaSuccess;
}

extern "C" bool runCudaExternalGaussianSmoke(void* mappedStorage)
{
    auto* gaussians = static_cast<GaussianGpuData*>(mappedStorage);

    cudaGaussianSmoke<<<1, kGaussianSmokeCount>>>(gaussians);

    if (cudaGetLastError() != cudaSuccess)
    {
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        return false;
    }

    GaussianGpuData hostGaussians[kGaussianSmokeCount]{};

    if (cudaMemcpy(hostGaussians, gaussians, sizeof(hostGaussians), cudaMemcpyDeviceToHost) != cudaSuccess)
    {
        return false;
    }

    const float expectedX[kGaussianSmokeCount] = { 0.0f, -0.5f, 0.5f };

    const float expectedColor[kGaussianSmokeCount][3] =
    {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    for (unsigned int i = 0; i < kGaussianSmokeCount; ++i)
    {
        const GaussianGpuData& gaussian = hostGaussians[i];

        if (gaussian.position[0] != expectedX[i] ||
            gaussian.position[1] != 0.0f ||
            gaussian.position[2] != 0.0f ||

            gaussian.opacity != 1.0f ||

            gaussian.scale[0] != 0.25f ||
            gaussian.scale[1] != 0.25f ||
            gaussian.scale[2] != 0.25f ||

            gaussian.rotation[0] != 1.0f ||
            gaussian.rotation[1] != 0.0f ||
            gaussian.rotation[2] != 0.0f ||
            gaussian.rotation[3] != 0.0f ||

            gaussian.color[0] != expectedColor[i][0] ||
            gaussian.color[1] != expectedColor[i][1] ||
            gaussian.color[2] != expectedColor[i][2])
        {
            return false;
        }
    }

    return true;
}