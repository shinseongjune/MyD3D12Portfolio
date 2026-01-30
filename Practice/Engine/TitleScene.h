#pragma once
#include "Scene.h"

class TitleScene : public Scene
{
public:
    void OnLoad(SceneContext& ctx) override;
    void OnUnload(SceneContext& ctx) override;
    void OnUpdate(SceneContext& ctx) override;

private:
    enum class TitleMenu
    {
        Start,
        Exit,
    };

    TextureHandle LoadTexture(SceneContext& ctx, const std::string& path);

    TitleMenu m_selected = TitleMenu::Start;
    EntityId m_arrows;

    float m_currentCooldown = 0.0f;
    float m_inputCooldown = 0.2f;

    enum class TitleState
    {
        Idle,           // 메뉴 조작 가능
        PendingStart,   // 시작 선택 후 사운드/딜레이 대기
        PendingExit     // 종료 선택 후 사운드/딜레이 대기
    };

    TitleState m_state = TitleState::Idle;
    float m_actionTimer = 0.0f;     // 남은 시간(초)

    SoundHandle m_arrowSound;
    SoundHandle m_enterSound;

    TextureHandle texh_black_w;
    TextureHandle texh_black_s;
    TextureHandle texh_black_8;
    TextureHandle texh_black_2;
    TextureHandle texh_black_up;
    TextureHandle texh_black_down;
    TextureHandle texh_black_enter;

    TextureHandle texh_white_w;
    TextureHandle texh_white_s;
    TextureHandle texh_white_8;
    TextureHandle texh_white_2;
    TextureHandle texh_white_up;
    TextureHandle texh_white_down;
    TextureHandle texh_white_enter;

    EntityId key_w;
    EntityId key_s;
    EntityId key_8;
    EntityId key_2;
    EntityId key_up;
    EntityId key_down;
    EntityId key_enter;

};