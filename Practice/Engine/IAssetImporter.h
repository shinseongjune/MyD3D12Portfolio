#pragma once
#include <string>
#include "AssetTypes.h"

class IAssetImporter
{
public:
    virtual ~IAssetImporter() = default;

    // "obj", "gltf", "glb" 같은 확장자(점 없이, 소문자 기준)
    virtual bool CanImportExtension(const std::string& extLower) const = 0;

    // 실제 import (CPU 데이터만 생성)
    virtual Result<ImportedModel> Import(const std::string& filePath,
        const ImportOptions& options) = 0;

    // 디버그/로그용
    virtual const char* GetName() const = 0;
};
