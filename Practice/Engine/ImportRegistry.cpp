#include "ImportRegistry.h"
#include <algorithm>

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string ImportRegistry::GetExtensionLower(const std::string& filePath)
{
    auto pos = filePath.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= filePath.size())
        return {};
    return ToLower(filePath.substr(pos + 1));
}

void ImportRegistry::Register(std::unique_ptr<IAssetImporter> importer)
{
    m_importers.push_back(std::move(importer));
}

IAssetImporter* ImportRegistry::FindImporterForExtensionLower(const std::string& extLower) const
{
    for (auto& it : m_importers)
    {
        if (it->CanImportExtension(extLower))
            return it.get();
    }
    return nullptr;
}

IAssetImporter* ImportRegistry::FindImporterForFile(const std::string& filePath) const
{
    return FindImporterForExtensionLower(GetExtensionLower(filePath));
}
