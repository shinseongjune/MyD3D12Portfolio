#pragma once
#include <cstdint>

enum class Key : uint8_t
{
    // Letters
    A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G',
    H = 'H', I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N',
    O = 'O', P = 'P', Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U',
    V = 'V', W = 'W', X = 'X', Y = 'Y', Z = 'Z',

    // Digits (top row)
    Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
    Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

    // Arrows
    Left = 0x25,
    Up = 0x26,
    Right = 0x27,
    Down = 0x28,

    // Common controls
    Escape = 0x1B,
    Space = 0x20,
    Enter = 0x0D,
    Tab = 0x09,
    Backspace = 0x08,

    Shift = 0x10,
    Ctrl = 0x11,
    Alt = 0x12,

    // Function keys
    F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73,
    F5 = 0x74, F6 = 0x75, F7 = 0x76, F8 = 0x77,
    F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,

    // Numpad (keypad)
    NumPad0 = 0x60, NumPad1 = 0x61, NumPad2 = 0x62, NumPad3 = 0x63, NumPad4 = 0x64,
    NumPad5 = 0x65, NumPad6 = 0x66, NumPad7 = 0x67, NumPad8 = 0x68, NumPad9 = 0x69,

    NumPadMul = 0x6A,
    NumPadAdd = 0x6B,
    NumPadSub = 0x6D,
    NumPadDec = 0x6E,
    NumPadDiv = 0x6F,

    // Extra
    CapsLock = 0x14,
    PageUp = 0x21,
    PageDown = 0x22,
    End = 0x23,
    Home = 0x24,
    Insert = 0x2D,
    Delete = 0x2E,
};

class Input
{
public:
    void BeginFrame();
    void EndFrame();
    void Clear();

    void OnKeyDown(uint32_t vk, bool isRepeat);
    void OnKeyUp(uint32_t vk);

    void OnMouseMove(int x, int y);
    void OnMouseButtonDown(int btn);
    void OnMouseButtonUp(int btn);

    bool IsKeyDown(Key k) const;
    bool IsKeyPressed(Key k) const;
    bool IsKeyReleased(Key k) const;

private:
    bool m_curr[256]{};
    bool m_prev[256]{};

    // Mouse
    int  m_mouseX = 0, m_mouseY = 0;
    int  m_mouseDX = 0, m_mouseDY = 0;
    bool m_mouseCurr[8]{};
    bool m_mousePrev[8]{};
};