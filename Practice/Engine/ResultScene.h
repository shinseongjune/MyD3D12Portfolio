#pragma once
#include "Scene.h"

class ResultScene : public Scene
{
public:
	// Scene을(를) 통해 상속됨
	void OnLoad(SceneContext& ctx) override;
	void OnUnload(SceneContext& ctx) override;
	void OnUpdate(SceneContext& ctx) override;

private:
    enum class ResultMenu
    {
        Resume,
        ToTitle,
    };

    TextureHandle LoadTexture(SceneContext& ctx, const std::string& path);

    ResultMenu m_selected = ResultMenu::Resume;
    EntityId m_arrows;

    float m_currentCooldown = 0.0f;
    float m_inputCooldown = 0.2f;

    enum class ResultState
    {
        Idle,           // 메뉴 조작 가능
        PendingResume,  // 재시작 선택 후 사운드/딜레이 대기
        PendingTitle    // 타이틀 선택 후 사운드/딜레이 대기
    };

    ResultState m_state = ResultState::Idle;
    float m_actionTimer = 0.0f;     // 남은 시간(초)

    SoundHandle m_arrowSound;
    SoundHandle m_enterSound;

};