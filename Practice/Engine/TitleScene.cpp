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

    // keys
    {
        texh_black_w = LoadTexture(ctx, "Assets/Texture/MenuScene/black_w.png");
        texh_black_s = LoadTexture(ctx, "Assets/Texture/MenuScene/black_s.png");
        texh_black_8 = LoadTexture(ctx, "Assets/Texture/MenuScene/black_8.png");
        texh_black_2 = LoadTexture(ctx, "Assets/Texture/MenuScene/black_2.png");
        texh_black_up = LoadTexture(ctx, "Assets/Texture/MenuScene/black_up.png");
        texh_black_down = LoadTexture(ctx, "Assets/Texture/MenuScene/black_down.png");
        texh_black_enter = LoadTexture(ctx, "Assets/Texture/MenuScene/black_enter.png");

        texh_white_w = LoadTexture(ctx, "Assets/Texture/MenuScene/white_w.png");
        texh_white_s = LoadTexture(ctx, "Assets/Texture/MenuScene/white_s.png");
        texh_white_8 = LoadTexture(ctx, "Assets/Texture/MenuScene/white_8.png");
        texh_white_2 = LoadTexture(ctx, "Assets/Texture/MenuScene/white_2.png");
        texh_white_up = LoadTexture(ctx, "Assets/Texture/MenuScene/white_up.png");
        texh_white_down = LoadTexture(ctx, "Assets/Texture/MenuScene/white_down.png");
        texh_white_enter = LoadTexture(ctx, "Assets/Texture/MenuScene/white_enter.png");

        {
            key_w = ctx.Instantiate("key_w");
            UIElementComponent image;
            image.texture = texh_white_w;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 576 };
            world.AddUIElement(key_w, image);
        }

        {
            key_s = ctx.Instantiate("key_s");
            UIElementComponent image;
            image.texture = texh_white_s;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 576 };
            world.AddUIElement(key_s, image);
        }

        {
            key_8 = ctx.Instantiate("key_8");
            UIElementComponent image;
            image.texture = texh_white_8;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 552 };
            world.AddUIElement(key_8, image);
        }

        {
            key_2 = ctx.Instantiate("key_2");
            UIElementComponent image;
            image.texture = texh_white_2;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 552 };
            world.AddUIElement(key_2, image);
        }

        {
            key_up = ctx.Instantiate("key_up");
            UIElementComponent image;
            image.texture = texh_white_up;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 10, 528 };
            world.AddUIElement(key_up, image);
        }

        {
            key_down = ctx.Instantiate("key_down");
            UIElementComponent image;
            image.texture = texh_white_down;
            image.sizePx = { 24, 24 };
            image.anchoredPosPx = { 34, 528 };
            world.AddUIElement(key_down, image);
        }

        {
            key_enter = ctx.Instantiate("key_enter");
            UIElementComponent image;
            image.texture = texh_white_enter;
            image.sizePx = { 48, 24 };
            image.anchoredPosPx = { 140, 576 };
            world.AddUIElement(key_enter, image);
        }
    }
}

void TitleScene::OnUnload(SceneContext& ctx)
{
}

void TitleScene::OnUpdate(SceneContext& ctx)
{
    ctx.DrawWText(400, 410, L"게임시작", 24.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(400, 480, L"게임종료", 24.0f, { 1, 1, 1, 1 });

    ctx.DrawWText(58, 576, L"메뉴선택", 18.0f, { 1, 1, 1, 1 });
    ctx.DrawWText(188, 576, L"확인", 18.0f, { 1, 1, 1, 1 });

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

    // keys
    {
        // w
        {
            if (ctx.input.IsKeyPressed(Key::W))
            {
                auto& image = ctx.world.GetUIElement(key_w);
                image.texture = texh_black_w;
            }
            else if (ctx.input.IsKeyReleased(Key::W))
            {
                auto& image = ctx.world.GetUIElement(key_w);
                image.texture = texh_white_w;
            }
        }

        // s
        {
            if (ctx.input.IsKeyPressed(Key::S))
            {
                auto& image = ctx.world.GetUIElement(key_s);
                image.texture = texh_black_s;
            }
            else if (ctx.input.IsKeyReleased(Key::S))
            {
                auto& image = ctx.world.GetUIElement(key_s);
                image.texture = texh_white_s;
            }
        }

        // 8
        {
            if (ctx.input.IsKeyPressed(Key::NumPad8))
            {
                auto& image = ctx.world.GetUIElement(key_8);
                image.texture = texh_black_8;
            }
            else if (ctx.input.IsKeyReleased(Key::NumPad8))
            {
                auto& image = ctx.world.GetUIElement(key_8);
                image.texture = texh_white_8;
            }
        }

        // 2
        {
            if (ctx.input.IsKeyPressed(Key::NumPad2))
            {
                auto& image = ctx.world.GetUIElement(key_2);
                image.texture = texh_black_2;
            }
            else if (ctx.input.IsKeyReleased(Key::NumPad2))
            {
                auto& image = ctx.world.GetUIElement(key_2);
                image.texture = texh_white_2;
            }
        }

        // up
        {
            if (ctx.input.IsKeyPressed(Key::Up))
            {
                auto& image = ctx.world.GetUIElement(key_up);
                image.texture = texh_black_up;
            }
            else if (ctx.input.IsKeyReleased(Key::Up))
            {
                auto& image = ctx.world.GetUIElement(key_up);
                image.texture = texh_white_up;
            }
        }

        // down
        {
            if (ctx.input.IsKeyPressed(Key::Down))
            {
                auto& image = ctx.world.GetUIElement(key_down);
                image.texture = texh_black_down;
            }
            else if (ctx.input.IsKeyReleased(Key::Down))
            {
                auto& image = ctx.world.GetUIElement(key_down);
                image.texture = texh_white_down;
            }
        }

        // enter
        {
            if (ctx.input.IsKeyPressed(Key::Enter))
            {
                auto& image = ctx.world.GetUIElement(key_enter);
                image.texture = texh_black_enter;
            }
            else if (ctx.input.IsKeyReleased(Key::Enter))
            {
                auto& image = ctx.world.GetUIElement(key_enter);
                image.texture = texh_white_enter;
            }
        }
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