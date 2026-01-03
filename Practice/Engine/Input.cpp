#include "Input.h"
#include <Windows.h>
#include <cstring>

static int ToVK(Key k)
{
    switch (k)
    {
    case Key::W: return 'W';
    case Key::A: return 'A';
    case Key::S: return 'S';
    case Key::D: return 'D';
    case Key::Q: return 'Q';
    case Key::E: return 'E';
	case Key::R: return 'R';
	case Key::G: return 'G';
    case Key::Up: return VK_UP;
    case Key::Down: return VK_DOWN;
    case Key::Left: return VK_LEFT;
    case Key::Right: return VK_RIGHT;
    case Key::Escape: return VK_ESCAPE;
	case Key::Space: return VK_SPACE;
    }
    return 0;
}

void Input::Update()
{
    memcpy(s_prev, s_curr, sizeof(s_curr));

    for (int i = 0; i < 256; ++i)
        s_curr[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
}

bool Input::IsKeyDown(Key k) const
{
    int vk = ToVK(k);
    return vk ? s_curr[vk] : false;
}

bool Input::IsKeyPressed(Key k) const
{
    int vk = ToVK(k);
    return vk ? (s_curr[vk] && !s_prev[vk]) : false;
}

bool Input::IsKeyReleased(Key k) const
{
    int vk = ToVK(k);
    return vk ? (!s_curr[vk] && s_prev[vk]) : false;
}
