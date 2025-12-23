#pragma once
#include <cstdint>
#include <Windows.h>

class Win32Window
{
public:
    Win32Window() = default;
    ~Win32Window() = default;

    // non-copyable
    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool Create(HINSTANCE hInstance, const wchar_t* title, uint32_t width, uint32_t height);
    void Destroy();

    // 메시지 처리: 종료 요청(WM_QUIT) 받으면 false 반환
    bool PumpMessages();

    HWND GetHwnd() const { return m_hwnd; }
    HINSTANCE GetHInstance() const { return m_hInstance; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();

private:
    HINSTANCE m_hInstance = nullptr;
    HWND      m_hwnd = nullptr;

    const wchar_t* m_title = L"Engine";
    const wchar_t* m_className = L"EngineWindowClass";

    uint32_t m_width = 1280;
    uint32_t m_height = 720;
};