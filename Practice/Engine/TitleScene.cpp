#include "TitleScene.h"
#include "SceneContext.h"

void TitleScene::OnLoad(SceneContext& ctx)
{
    auto& world = ctx.world;

    // 1) Camera
    EntityId cam = ctx.Instantiate("TitleCamera");
    world.AddCamera(cam);
    world.SetLocalPosition(cam, { 0.f, 0.f, -5.f });

    // 2) Title Image
    TextureHandle texh_title = LoadTexture(ctx, "Assets/Texture/title.png");

    EntityId title = ctx.Instantiate("TitleImage");
    UIElementComponent titleImage;
    titleImage.texture = texh_title;
    titleImage.sizePx = { 900, 600 };
    world.AddUIElement(title, titleImage);

    // 3) Menu Arrows
    TextureHandle texh_arrows = LoadTexture(ctx, "Assets/Texture/arrow.png");

    m_arrows = ctx.Instantiate("Arrows");
    UIElementComponent arrowsImage;
    arrowsImage.texture = texh_arrows;
    arrowsImage.sizePx = { 24, 24 };
    world.AddUIElement(m_arrows, arrowsImage);

    // 4) BGM
    auto bgm = ctx.LoadSoundScoped("Assets/Audio/title_bgm.mp3");
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

void TitleScene::OnUnload(SceneContext& ctx)
{
}

void TitleScene::OnUpdate(SceneContext& ctx)
{
    ctx.DrawWText(400, 410, L"게임시작", 24.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(400, 480, L"게임종료", 24.0f, { 1, 1, 1, 1 });

    if (m_state == TitleState::Idle && m_currentCooldown <= 0)
    {
        if (ctx.input.IsKeyDown(Key::W) || ctx.input.IsKeyDown(Key::Up) || ctx.input.IsKeyDown(Key::NumPad8))
        {
            m_selected = (TitleMenu)((int)m_selected - 1);
            if ((int)m_selected < 0) m_selected = TitleMenu::Exit;
            m_currentCooldown = m_inputCooldown;
            ctx.PlaySFX(m_arrowSound);
        }
        if (ctx.input.IsKeyDown(Key::S) || ctx.input.IsKeyDown(Key::Down) || ctx.input.IsKeyDown(Key::NumPad2))
        {
            m_selected = (TitleMenu)((int)m_selected + 1);
            if ((int)m_selected > (int)TitleMenu::Exit) m_selected = TitleMenu::Start;
            m_currentCooldown = m_inputCooldown;
            ctx.PlaySFX(m_arrowSound);
        }
    }
    m_currentCooldown = std::fmax(m_currentCooldown - ctx.dt, 0);

    auto& arrowsImage = ctx.world.GetUIElement(m_arrows);

    switch (m_selected)
    {
    case TitleMenu::Start:
        arrowsImage.anchoredPosPx = { 374, 412 };
        break;
    case TitleMenu::Exit:
        arrowsImage.anchoredPosPx = { 374, 482 };
        break;
    default:
        arrowsImage.anchoredPosPx = { 374, 482 };
    }

    if (m_state == TitleState::Idle && ctx.input.IsKeyDown(Key::Enter))
    {
        m_state = (m_selected == TitleMenu::Start) ? TitleState::PendingStart : TitleState::PendingExit;

        m_actionTimer = 1.8f;

        ctx.PlaySFX(m_enterSound);
    }

    if (m_state != TitleState::Idle)
    {
        m_actionTimer = std::fmax(m_actionTimer - ctx.dt, 0.0f);

        if (m_actionTimer <= 0.0f)
        {
            if (m_state == TitleState::PendingStart)
            {
                ctx.StopBGM();
                ctx.RequestLoadScene(SceneId::Play);
            }
            else // PendingExit
            {
                ctx.RequestExit();
            }
        }
    }
}

TextureHandle TitleScene::LoadTexture(SceneContext& ctx, const std::string& path)
{
    auto tex = ctx.LoadTextureScoped(path);
    return tex.value;
}