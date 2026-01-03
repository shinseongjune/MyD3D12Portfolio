#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

struct Float2 { float x, y; };
struct Float3 { float x, y, z; };
struct Float4 { float x, y, z, w; };

struct AABBBound
{
    Float3 min{ +1e30f, +1e30f, +1e30f };
    Float3 max{ -1e30f, -1e30f, -1e30f };
};

inline void ExpandAABB(AABBBound& b, const Float3& p)
{
    if (p.x < b.min.x) b.min.x = p.x;
    if (p.y < b.min.y) b.min.y = p.y;
    if (p.z < b.min.z) b.min.z = p.z;
    if (p.x > b.max.x) b.max.x = p.x;
    if (p.y > b.max.y) b.max.y = p.y;
    if (p.z > b.max.z) b.max.z = p.z;
}

// 엔진이 기대하는 "CPU 버텍스 표준"
// (나중에 tangent/bitangent, color, joints/weights 등 확장)
struct ImportedVertex
{
    Float3 position{};
    Float3 normal{ 0, 1, 0 };
    Float2 uv{ 0, 0 };
    Float4 tangent{ 1, 0, 0, 1 }; // xyz=tangent, w=handedness
};

struct ImportedSubmesh
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
    std::string name;
};

enum class ImageColorSpace : uint8_t
{
    Linear,
    SRGB
};

struct ImportedImageRef
{
    std::string uri;                 // 파일 경로(상대/절대)
    ImageColorSpace colorSpace = ImageColorSpace::SRGB;
};

struct ImportedMaterial
{
    std::string name;

    Float4 baseColorFactor{ 1, 1, 1, 1 };
    std::optional<uint32_t> baseColorImage; // images[] 인덱스

    // 확장: normalImage, metallicRoughnessImage, emissiveImage ...
};

struct ImportedMesh
{
    std::string name;

    std::vector<ImportedVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<ImportedSubmesh> submeshes;

    AABBBound bounds{};
};

struct ImportedModel
{
    // “파일 1개”를 import한 결과물 전체
    std::string sourcePath;
    std::vector<ImportedImageRef> images;
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedMesh> meshes;
};

struct ImportOptions
{
    bool flipV = true;
    bool triangulate = true;

    bool generateNormalsIfMissing = true;
    bool generateTangentsIfMissing = false;

    float uniformScale = 1.0f;
};

struct ImportError
{
    std::string message;
};

template<typename T>
struct Result
{
    T value{};
    std::optional<ImportError> error;

    static Result Ok(T v) { Result r; r.value = std::move(v); return r; }
    static Result Fail(std::string msg) { Result r; r.error = ImportError{ std::move(msg) }; return r; }

    bool IsOk() const { return !error.has_value(); }
};
