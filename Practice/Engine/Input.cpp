#include "Input.h"
#include <cstring>

static int ToVK(Key k)
{
    return static_cast<int>(k);
}

void Input::BeginFrame()
{
    m_mouseDX = 0;
    m_mouseDY = 0;
}

void Input::EndFrame()
{
    memcpy(m_prev, m_curr, sizeof(m_curr));
    memcpy(m_mousePrev, m_mouseCurr, sizeof(m_mouseCurr));
}

void Input::Clear()
{
    memset(m_curr, 0, sizeof(m_curr));
    memset(m_prev, 0, sizeof(m_prev));
    memset(m_mouseCurr, 0, sizeof(m_mouseCurr));
    memset(m_mousePrev, 0, sizeof(m_mousePrev));
    m_mouseDX = m_mouseDY = 0;
}

void Input::OnKeyDown(uint32_t vk, bool isRepeat)
{
    if (vk < 256)
        m_curr[vk] = true;
}

void Input::OnKeyUp(uint32_t vk)
{
    if (vk < 256)
        m_curr[vk] = false;
}

void Input::OnMouseMove(int x, int y)
{
    m_mouseDX += (x - m_mouseX);
    m_mouseDY += (y - m_mouseY);
    m_mouseX = x;
    m_mouseY = y;
}

void Input::OnMouseButtonDown(int btn) { if (btn < 8) m_mouseCurr[btn] = true; }
void Input::OnMouseButtonUp(int btn) { if (btn < 8) m_mouseCurr[btn] = false; }

bool Input::IsKeyDown(Key k) const
{
    int vk = ToVK(k);
    return vk ? m_curr[vk] : false;
}

bool Input::IsKeyPressed(Key k) const
{
    int vk = ToVK(k);
    return vk ? (m_curr[vk] && !m_prev[vk]) : false;
}

bool Input::IsKeyReleased(Key k) const
{
    int vk = ToVK(k);
    return vk ? (!m_curr[vk] && m_prev[vk]) : false;
}
