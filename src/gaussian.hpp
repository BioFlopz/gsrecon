#pragma once

#include <cstddef>
#include <type_traits>
#include <cstdint>

struct alignas(16) GaussianGpuData
{
    float position[3];
    float opacity;

    float scale[3];
    float padding0;

    // Quaternion order: w, x, y, z.
    float rotation[4];

    float color[3];
    float padding1;
};

static_assert(std::is_standard_layout_v<GaussianGpuData>);
static_assert(std::is_trivially_copyable_v<GaussianGpuData>);

static_assert(sizeof(GaussianGpuData) == 64);
static_assert(alignof(GaussianGpuData) == 16);

static_assert(offsetof(GaussianGpuData, position) == 0);
static_assert(offsetof(GaussianGpuData, opacity) == 12);
static_assert(offsetof(GaussianGpuData, scale) == 16);
static_assert(offsetof(GaussianGpuData, rotation) == 32);
static_assert(offsetof(GaussianGpuData, color) == 48);

struct alignas(16) GaussianPreprocessData
{
    float viewDepth;
    std::uint32_t visible;
    float padding[2];

    float covariance0[4]; // xx, xy, xz, yy
    float covariance1[4]; // yz, zz, padding, padding

    float conicRadius[4]; // conic.x, conic.y, conic.z, radiusPixels
};

static_assert(std::is_standard_layout_v<GaussianPreprocessData>);
static_assert(std::is_trivially_copyable_v<GaussianPreprocessData>);
static_assert(sizeof(GaussianPreprocessData) == 64);
static_assert(alignof(GaussianPreprocessData) == 16);
static_assert(offsetof(GaussianPreprocessData, viewDepth) == 0);
static_assert(offsetof(GaussianPreprocessData, visible) == 4);
static_assert(offsetof(GaussianPreprocessData, covariance0) == 16);
static_assert(offsetof(GaussianPreprocessData, covariance1) == 32);
static_assert(offsetof(GaussianPreprocessData, conicRadius) == 48);