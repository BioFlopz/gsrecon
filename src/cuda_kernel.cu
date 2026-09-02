#include <cuda_runtime.h>

#include "gaussian.hpp"


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
    if (threadIdx.x != 0)
    {
        return;
    }

    GaussianGpuData& gaussian = gaussians[0];

    gaussian.position[0] = 0.0f;
    gaussian.position[1] = 0.0f;
    gaussian.position[2] = 0.0f;
    gaussian.opacity = 1.0f;

    gaussian.scale[0] = 0.25f;
    gaussian.scale[1] = 0.25f;
    gaussian.scale[2] = 0.25f;
    gaussian.padding0 = 0.0f;

    // Identity quaternion: w, x, y, z.
    gaussian.rotation[0] = 1.0f;
    gaussian.rotation[1] = 0.0f;
    gaussian.rotation[2] = 0.0f;
    gaussian.rotation[3] = 0.0f;

    gaussian.color[0] = 1.0f;
    gaussian.color[1] = 0.0f;
    gaussian.color[2] = 0.0f;
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

    cudaGaussianSmoke<<<1, 1>>>(gaussians);

    return cudaGetLastError() == cudaSuccess;
}

extern "C" bool runCudaExternalGaussianSmoke(void* mappedStorage)
{
    auto* gaussians = static_cast<GaussianGpuData*>(mappedStorage);

    cudaGaussianSmoke<<<1, 1>>>(gaussians);

    if (cudaGetLastError() != cudaSuccess)
    {
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        return false;
    }

    GaussianGpuData hostGaussian{};

    if (cudaMemcpy(
            &hostGaussian,
            gaussians,
            sizeof(hostGaussian),
            cudaMemcpyDeviceToHost) != cudaSuccess)
    {
        return false;
    }

    return
        hostGaussian.position[0] == 0.0f &&
        hostGaussian.position[1] == 0.0f &&
        hostGaussian.position[2] == 0.0f &&
        hostGaussian.opacity == 1.0f &&
        hostGaussian.scale[0] == 0.25f &&
        hostGaussian.scale[1] == 0.25f &&
        hostGaussian.scale[2] == 0.25f &&
        hostGaussian.rotation[0] == 1.0f &&
        hostGaussian.rotation[1] == 0.0f &&
        hostGaussian.rotation[2] == 0.0f &&
        hostGaussian.rotation[3] == 0.0f &&
        hostGaussian.color[0] == 1.0f &&
        hostGaussian.color[1] == 0.0f &&
        hostGaussian.color[2] == 0.0f;
}