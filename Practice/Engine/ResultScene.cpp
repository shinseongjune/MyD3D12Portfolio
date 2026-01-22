#include "ResultScene.h"

void ResultScene::OnLoad(SceneContext& ctx)
{
    auto& world = ctx.world;

    // 1) Camera
    EntityId cam = ctx.Instantiate("TitleCamera");
    world.AddCamera(cam);
    world.SetLocalPosition(cam, { 0.f, 0.f, -5.f });

    // 2) Title Image
    TextureHandle texh_bg;
    if (GameManager::GetInstance().CurrentState() == GameManager::State::Win) 
    {
        texh_bg = LoadTexture(ctx, "Assets/Texture/win.png");
    }
    else // lose
    {
        texh_bg = LoadTexture(ctx, "Assets/Texture/lose.png");
    }

    EntityId bg = ctx.Instantiate("ResultImage");
    UIElementComponent bgImage;
    bgImage.texture = texh_bg;
    bgImage.sizePx = { 900, 600 };
    world.AddUIElement(bg, bgImage);

    // 3) Menu Arrows
    TextureHandle texh_arrows = LoadTexture(ctx, "Assets/Texture/arrow.png");

    m_arrows = ctx.Instantiate("Arrows");
    UIElementComponent arrowsImage;
    arrowsImage.texture = texh_arrows;
    arrowsImage.sizePx = { 24, 24 };
    world.AddUIElement(m_arrows, arrowsImage);

    // 4) BGM
    Result<SoundHandle> bgm;
    if (GameManager::GetInstance().CurrentState() == GameManager::State::Win)
    {
        bgm = ctx.LoadSoundScoped("Assets/Audio/win_bgm.mp3");
    }
    else
    {
        bgm = ctx.LoadSoundScoped("Assets/Audio/lose_bgm.mp3");
    }
    if (bgm.IsOk())
        ctx.PlayBGM(bgm.value, 1.0f);

    // 5) SFX
    auto arrowSound = ctx.LoadSoundScoped("Assets/Audio/title_menu_arrow.mp3");
    if (arrowSound.IsOk())
        m_arrowSound = arrowSound.value;
    auto enterSound = ctx.LoadSoundScoped("Assets/Audio/title_menu_enter.mp3");
    if (enterSound.IsOk())
        m_enterSound = enterSound.value;
}

void ResultScene::OnUnload(SceneContext& ctx)
{
}

void ResultScene::OnUpdate(SceneContext& ctx)
{
    ctx.DrawWText(400, 410, L"재시작", 24.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(400, 480, L"타이틀", 24.0f, { 1, 1, 1, 1 });

    if (m_state == ResultState::Idle && m_currentCooldown <= 0)
    {
        if (ctx.input.IsKeyDown(Key::W) || ctx.input.IsKeyDown(Key::Up) || ctx.input.IsKeyDown(Key::NumPad8))
        {
            m_selected = (ResultMenu)((int)m_selected - 1);
            if ((int)m_selected < 0) m_selected = ResultMenu::ToTitle;
            m_currentCooldown = m_inputCooldown;
            ctx.PlaySFX(m_arrowSound);
        }
        if (ctx.input.IsKeyDown(Key::S) || ctx.input.IsKeyDown(Key::Down) || ctx.input.IsKeyDown(Key::NumPad2))
        {
            m_selected = (ResultMenu)((int)m_selected + 1);
            if ((int)m_selected > (int)ResultMenu::ToTitle) m_selected = ResultMenu::Resume;
            m_currentCooldown = m_inputCooldown;
            ctx.PlaySFX(m_arrowSound);
        }
    }
    m_currentCooldown = std::fmax(m_currentCooldown - ctx.dt, 0);

    auto& arrowsImage = ctx.world.GetUIElement(m_arrows);

    switch (m_selected)
    {
    case ResultMenu::Resume:
        arrowsImage.anchoredPosPx = { 374, 412 };
        break;
    case ResultMenu::ToTitle:
        arrowsImage.anchoredPosPx = { 374, 482 };
        break;
    default:
        arrowsImage.anchoredPosPx = { 374, 482 };
    }

    if (m_state == ResultState::Idle && ctx.input.IsKeyDown(Key::Enter))
    {
        m_state = (m_selected == ResultMenu::Resume) ? ResultState::PendingResume : ResultState::PendingTitle;

        m_actionTimer = 1.8f;

        ctx.PlaySFX(m_enterSound);
    }

    if (m_state != ResultState::Idle)
    {
        m_actionTimer = std::fmax(m_actionTimer - ctx.dt, 0.0f);

        if (m_actionTimer <= 0.0f)
        {
            if (m_state == ResultState::PendingResume)
            {
                ctx.StopBGM();
                ctx.RequestLoadScene(SceneId::Play);
            }
            else // PendingTitle
            {
                ctx.RequestLoadScene(SceneId::Title);
            }
        }
    }
}

TextureHandle ResultScene::LoadTexture(SceneContext& ctx, const std::string& path)
{
    auto tex = ctx.LoadTextureScoped(path);
    return tex.value;
}
