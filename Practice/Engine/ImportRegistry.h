#pragma once
#include <memory>
#include <vector>
#include <string>
#include "IAssetImporter.h"

class ImportRegistry
{
public:
    void Register(std::unique_ptr<IAssetImporter> importer);

    // 파일 경로로 importer 선택
    IAssetImporter* FindImporterForFile(const std::string& filePath) const;

    // 확장자 직접 지정
    IAssetImporter* FindImporterForExtensionLower(const std::string& extLower) const;

private:
    std::vector<std::unique_ptr<IAssetImporter>> m_importers;

    static std::string GetExtensionLower(const std::string& filePath);
};
