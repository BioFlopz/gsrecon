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

[[vk::binding(0, 0)]]
StructuredBuffer<GaussianGpuData> gaussians;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraGpuData> camera;

[[vk::binding(2, 0)]]
StructuredBuffer<GaussianPreprocessData> preprocessData;

struct Covariance3D
{
    float xx;
    float xy;
    float xz;
    float yy;
    float yz;
    float zz;
};



float3 computeGaussianScreenCovariance(GaussianGpuData gaussian, Covariance3D covariance3D)
{
    //
    // Reference sequence:
    //
    // transformPoint4x3
    //     -> FOV clamp
    //     -> J
    //     -> W
    //     -> T = W * J
    //     -> Vrk
    //     -> transpose(T) * transpose(Vrk) * T
    //     -> +0.3 low-pass
    //

    float3 t = mul(float4(gaussian.position, 1.0f), camera.view).xyz;

    const float projectionX = camera.projection[0][0];
    const float projectionY = camera.projection[1][1];
    const float tanFovX = 1.0f / projectionX;
    const float tanFovY = 1.0f / projectionY;
    const float focalX = 0.5f * camera.viewportSize.x * projectionX;
    const float focalY = 0.5f * camera.viewportSize.y * projectionY;
    const float limitX = 1.3f * tanFovX;
    const float limitY = 1.3f * tanFovY;
    const float xOverZ = t.x / t.z;
    const float yOverZ = t.y / t.z;

    t.x = clamp(xOverZ, -limitX, limitX) * t.z;
    t.y = clamp(yOverZ, -limitY, limitY) * t.z;


    float3x3 J = (float3x3)0.0f;

    J[0][0] = focalX / t.z;
    J[1][1] = focalY / t.z;
    J[2][0] = -(focalX * t.x) / (t.z * t.z);
    J[2][1] = -(focalY * t.y) / (t.z * t.z);


    float3x3 W;

    W[0][0] = camera.view[0][0];
    W[0][1] = camera.view[0][1];
    W[0][2] = camera.view[0][2];

    W[1][0] = camera.view[1][0];
    W[1][1] = camera.view[1][1];
    W[1][2] = camera.view[1][2];

    W[2][0] = camera.view[2][0];
    W[2][1] = camera.view[2][1];
    W[2][2] = camera.view[2][2];


    const float3x3 T = mul(W, J);


    float3x3 Vrk;

    Vrk[0][0] = covariance3D.xx;
    Vrk[0][1] = covariance3D.xy;
    Vrk[0][2] = covariance3D.xz;

    Vrk[1][0] = covariance3D.xy;
    Vrk[1][1] = covariance3D.yy;
    Vrk[1][2] = covariance3D.yz;

    Vrk[2][0] = covariance3D.xz;
    Vrk[2][1] = covariance3D.yz;
    Vrk[2][2] = covariance3D.zz;


    float3x3 covariance =
        mul(
            transpose(T),
            mul(
                transpose(Vrk),
                T));


    //
    // Reference low-pass filter.
    //

    covariance[0][0] += 0.3f;
    covariance[1][1] += 0.3f;


    return float3(
        covariance[0][0],
        covariance[0][1],
        covariance[1][1]);
}


bool computeGaussianProjectedFootprint(float3 covariance2D, out float3 conic, out float radiusPixels)
{
    //
    // Reference:
    // det -> validity -> inverse/conic
    //     -> eigenvalues -> 3-sigma radius
    //

    const float determinant = covariance2D.x * covariance2D.z - covariance2D.y * covariance2D.y;

    if (determinant == 0.0f)
    {
        conic = float3(0.0f, 0.0f, 0.0f);
        radiusPixels = 0.0f;

        return false;
    }

    const float determinantInverse = 1.0f / determinant;

    conic = float3(covariance2D.z * determinantInverse, -covariance2D.y * determinantInverse, covariance2D.x * determinantInverse);


    const float mid = 0.5f * (covariance2D.x + covariance2D.z);

    const float eigenvalueOffset = sqrt(max(0.1f, mid * mid - determinant));

    const float lambda1 = mid + eigenvalueOffset;
    const float lambda2 = mid - eigenvalueOffset;

    radiusPixels = ceil(3.0f * sqrt(max(lambda1, lambda2)));

    return true;
}


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


    const float4 worldCenter = float4(gaussian.position, 1.0f);
    const float4 viewCenter = mul(worldCenter, camera.view);

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

    const float4 clipCenter = mul(viewCenter, camera.projection);


    //
    // Project the Gaussian's real 3D covariance into screen space.
    //

    Covariance3D covariance3D;

    covariance3D.xx = preprocess.covariance0.x;
    covariance3D.xy = preprocess.covariance0.y;
    covariance3D.xz = preprocess.covariance0.z;
    covariance3D.yy = preprocess.covariance0.w;

    covariance3D.yz = preprocess.covariance1.x;
    covariance3D.zz = preprocess.covariance1.y;

    const float3 covariance2D = computeGaussianScreenCovariance(gaussian, covariance3D);
    float3 conic;
    float radiusPixels;

    const bool footprintValid =computeGaussianProjectedFootprint(covariance2D, conic, radiusPixels);

    if (!footprintValid)
    {
        radiusPixels = 0.0f;
    }


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