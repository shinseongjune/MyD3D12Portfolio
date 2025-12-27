#pragma once
#include "IAssetImporter.h"

class ObjImporter_Minimal final : public IAssetImporter
{
public:
    bool CanImportExtension(const std::string& extLower) const override
    {
        return extLower == "obj";
    }

    Result<ImportedModel> Import(const std::string& filePath,
        const ImportOptions& options) override;

    const char* GetName() const override { return "ObjImporter_Minimal"; }
};
