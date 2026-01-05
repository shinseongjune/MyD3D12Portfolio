#pragma once
#include <string>
#include "Utilities.h"
#include "SoundClip.h"

class SoundImporterMF
{
public:
	Result<SoundClip> DecodeToPCM(const std::string& path);
};