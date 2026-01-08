#include "SceneContext.h"

Result<TextureHandle> SceneContext::LoadCubemapScoped(const std::array<std::string, 6>& utf8Paths)
{
    auto r = textures.LoadCubemap(utf8Paths);
    if (r.IsOk())
        scope.Track(r.value);
    return r;
}

EntityId SceneContext::Instantiate(const std::string& name)
{
    EntityId e = world.CreateEntity(name);
    scope.Track(e);
    return e;
}

void SceneContext::Destroy(EntityId e)
{
    if (world.IsAlive(e))
        world.RequestDestroy(e);
    // scope에서 제거까지 굳이 안 해도 됨(중복 요청은 Flush에서 처리하면 됨)
}

Result<ModelAsset> SceneContext::ImportModel(const std::string& path, const ImportOptions& importOpt)
{
    auto imported = assets.ImportModel(path, importOpt);
	if (!imported.IsOk())
        return Result<ModelAsset>::Fail(imported.error->message);
    
    return imported;
}

Result<EntityId> SceneContext::SpawnModel(const std::string& path, const ImportOptions& importOpt, const SpawnModelOptions& spawnOpt)
{
    auto imported = assets.ImportModel(path, importOpt);
    if (!imported.IsOk())
        return Result<EntityId>::Fail(imported.error->message);

    auto spawned = assets.InstantiateModel(world, imported.value, spawnOpt);
    if (!spawned.IsOk())
        return Result<EntityId>::Fail(spawned.error->message);

    scope.Track(spawned.value);
    return spawned;
}

Result<EntityId> SceneContext::SpawnModel(const ModelAsset& asset, const SpawnModelOptions& spawnOpt)
{
    auto spawned = assets.InstantiateModel(world, asset, spawnOpt);
    if (!spawned.IsOk())
        return Result<EntityId>::Fail(spawned.error->message);
    scope.Track(spawned.value);
    return spawned;
}

Result<TextureHandle> SceneContext::LoadTextureScoped(const std::string& utf8Path)
{
    auto r = textures.Load(utf8Path);
    if (r.IsOk())
        scope.Track(r.value);
    return r;
}

Result<TextureHandle> SceneContext::LoadTextureShared(const std::string& utf8Path)
{
    // 추적 안 함: 공유 리소스로 취급
    return textures.Load(utf8Path);
}

Result<SoundHandle> SceneContext::LoadSoundScoped(const std::string& utf8Path)
{
    SoundImporterMF importer;
    auto r = importer.DecodeToPCM(utf8Path);
    if (!r.IsOk())
        return Result<SoundHandle>::Fail(r.error->message);

    SoundHandle h = sounds.Create(r.value);
    scope.Track(h);
    return Result<SoundHandle>::Ok(h);
}

Result<SoundHandle> SceneContext::LoadSoundShared(const std::string& utf8Path)
{
    // 공유 리소스 취급: scope에 Track 안 함
    SoundImporterMF importer;
    auto r = importer.DecodeToPCM(utf8Path);
    if (!r.IsOk())
        return Result<SoundHandle>::Fail(r.error->message);

    SoundHandle h = sounds.Create(r.value);
    return Result<SoundHandle>::Ok(h);
}

void SceneContext::PlaySFX(SoundHandle clip, float volume, float pitch)
{
    AudioPlayDesc d{};
    d.volume = volume;
    d.pitch = pitch;
    d.loop = false;
    d.bus = (uint8_t)AudioBus::SFX;
    audio.PlayOneShot(clip, d);
}

void SceneContext::PlayBGM(SoundHandle clip, float volume)
{
    audio.PlayBGM(clip, volume);
}

void SceneContext::StopBGM()
{
    audio.StopBGM();
}
void SceneContext::DrawText(float x, float y, const std::wstring& str, float sizePx, const DirectX::XMFLOAT4& color, const std::wstring& fontFamily)
{
    UITextDraw t;
    t.x = x;
    t.y = y;
    t.sizePx = sizePx;
    t.color = color;
    t.text = str;
    t.fontFamily = fontFamily;
    text.push_back(std::move(t));
}
