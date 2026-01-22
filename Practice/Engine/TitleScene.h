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

};