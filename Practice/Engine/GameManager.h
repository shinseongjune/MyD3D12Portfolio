#pragma once

enum class SceneId
{
	Title,
	Play,
	Result,
};

class GameManager
{
public:
	enum class State
	{
		Playing,
		Win,
		Lose,
		Paused,
	};

	static GameManager& GetInstance() 
	{ 
		static GameManager instance;
		return instance;
	};
	State CurrentState() { return currentState; }
	void SetState(State s) { currentState = s; }

private:
	GameManager() = default;
	GameManager(const GameManager&) = delete;
	GameManager& operator=(const GameManager&) = delete;

	State currentState = State::Playing;

};