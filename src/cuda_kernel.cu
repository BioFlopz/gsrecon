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

__device__ void computeGaussianCovariance3D(const GaussianGpuData& gaussian, float* covariance)
{
    // GaussianGpuData quaternion order: w, x, y, z.
    //
    // This function expects a normalized quaternion.
    const float w = gaussian.rotation[0];
    const float x = gaussian.rotation[1];
    const float y = gaussian.rotation[2];
    const float z = gaussian.rotation[3];

    const float r00 = 1.0f - 2.0f * (y * y + z * z);
    const float r01 = 2.0f * (x * y - w * z);
    const float r02 = 2.0f * (x * z + w * y);

    const float r10 = 2.0f * (x * y + w * z);
    const float r11 = 1.0f - 2.0f * (x * x + z * z);
    const float r12 = 2.0f * (y * z - w * x);

    const float r20 = 2.0f * (x * z - w * y);
    const float r21 = 2.0f * (y * z + w * x);
    const float r22 = 1.0f - 2.0f * (x * x + y * y);

    const float sx2 = gaussian.scale[0] * gaussian.scale[0];
    const float sy2 = gaussian.scale[1] * gaussian.scale[1];
    const float sz2 = gaussian.scale[2] * gaussian.scale[2];

    //
    // Sigma = R * S^2 * transpose(R)
    //
    // Sigma is symmetric, so store only:
    // xx, xy, xz, yy, yz, zz
    //
    covariance[0] =
        r00 * r00 * sx2 +
        r01 * r01 * sy2 +
        r02 * r02 * sz2;

    covariance[1] =
        r00 * r10 * sx2 +
        r01 * r11 * sy2 +
        r02 * r12 * sz2;

    covariance[2] =
        r00 * r20 * sx2 +
        r01 * r21 * sy2 +
        r02 * r22 * sz2;

    covariance[3] =
        r10 * r10 * sx2 +
        r11 * r11 * sy2 +
        r12 * r12 * sz2;

    covariance[4] =
        r10 * r20 * sx2 +
        r11 * r21 * sy2 +
        r12 * r22 * sz2;

    covariance[5] =
        r20 * r20 * sx2 +
        r21 * r21 * sy2 +
        r22 * r22 * sz2;
}


__global__ void cudaGaussianCovarianceSmoke(float* covariance)
{
    GaussianGpuData gaussian{};

    gaussian.scale[0] = 2.0f;
    gaussian.scale[1] = 3.0f;
    gaussian.scale[2] = 4.0f;

    //
    // Normalized quaternion:
    // w = x = y = z = 0.5
    //
    // This produces a non-identity rotation, so this smoke proves
    // that rotation actually participates in the covariance.
    //
    gaussian.rotation[0] = 0.5f;
    gaussian.rotation[1] = 0.5f;
    gaussian.rotation[2] = 0.5f;
    gaussian.rotation[3] = 0.5f;

    computeGaussianCovariance3D(gaussian, covariance);
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
    gaussian.position[2] = index == 0 ? 0.0f : index == 1 ? 0.5f : 1.0f;

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
    const float expectedZ[kGaussianSmokeCount] = { 0.0f, 0.5f, 1.0f };

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
            gaussian.position[2] != expectedZ[i] ||

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


extern "C" bool runCudaGaussianCovarianceSmoke()
{
    float* deviceCovariance = nullptr;

    if (cudaMalloc(&deviceCovariance, 6 * sizeof(float)) != cudaSuccess)
    {
        return false;
    }

    cudaGaussianCovarianceSmoke<<<1, 1>>>(deviceCovariance);

    if (cudaGetLastError() != cudaSuccess)
    {
        cudaFree(deviceCovariance);
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        cudaFree(deviceCovariance);
        return false;
    }

    float hostCovariance[6]{};

    const cudaError_t copyResult = cudaMemcpy(hostCovariance, deviceCovariance, sizeof(hostCovariance), cudaMemcpyDeviceToHost);

    cudaFree(deviceCovariance);

    if (copyResult != cudaSuccess)
    {
        return false;
    }

    //
    // For scale = (2, 3, 4) and q = (0.5, 0.5, 0.5, 0.5):
    //
    // Sigma =
    //     [16  0  0]
    //     [ 0  4  0]
    //     [ 0  0  9]
    //
    const float expected[6] =
    {
        16.0f,
        0.0f,
        0.0f,
        4.0f,
        0.0f,
        9.0f
    };

    for (unsigned int i = 0; i < 6; ++i)
    {
        if (hostCovariance[i] != expected[i])
        {
            return false;
        }
    }

    return true;
}