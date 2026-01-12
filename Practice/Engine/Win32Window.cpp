#include "Win32Window.h"
#include <windowsx.h>
#include "Input.h"

static uint32_t NormalizeNumpadVK(uint32_t vk, LPARAM lParam)
{
    const bool extended = (lParam & (1 << 24)) != 0;
    const uint32_t sc = (uint32_t)((lParam >> 16) & 0xFF); // scan code

    // 넘패드 숫자키(NumLock/Shift 상태에 따라 VK가 바뀌어도) scancode는 동일한 편
    // 그리고 "진짜 방향키"는 보통 extended(E0)가 켜져 들어온다.
    if (!extended)
    {
        switch (sc)
        {
        case 0x47: return VK_NUMPAD7; // Home
        case 0x48: return VK_NUMPAD8; // Up
        case 0x49: return VK_NUMPAD9; // PgUp
        case 0x4B: return VK_NUMPAD4; // Left
        case 0x4C: return VK_NUMPAD5; // Clear
        case 0x4D: return VK_NUMPAD6; // Right
        case 0x4F: return VK_NUMPAD1; // End
        case 0x50: return VK_NUMPAD2; // Down
        case 0x51: return VK_NUMPAD3; // PgDn
        case 0x52: return VK_NUMPAD0; // Ins
        case 0x53: return VK_DECIMAL; // Del (.)
        default: break;
        }
    }

    // 나머지는 원래 vk 유지
    return vk;
}


bool Win32Window::RegisterWindowClass()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Win32Window::StaticWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;         // ޴ (ø )
    wc.lpszClassName = m_className;
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    // ̹ ϵ   и    Ȯ
    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0)
    {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }
    return true;
}

bool Win32Window::Create(HINSTANCE hInstance, const wchar_t* title, uint32_t width, uint32_t height)
{
    m_hInstance = hInstance;
    m_title = title ? title : L"Engine";
    m_width = width;
    m_height = height;

    if (!RegisterWindowClass())
        return false;

    // Ŭ̾Ʈ  width/height ǵ ü â ũ 
    RECT rc{ 0, 0, (LONG)m_width, (LONG)m_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    const int winW = rc.right - rc.left;
    const int winH = rc.bottom - rc.top;

    m_hwnd = CreateWindowExW(
        0,
        m_className,
        m_title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        nullptr,
        nullptr,
        m_hInstance,
        this //  this ؼ WndProc 
    );

    if (!m_hwnd)
        return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void Win32Window::Destroy()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool Win32Window::PumpMessages()
{
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

LRESULT CALLBACK Win32Window::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32Window* self = nullptr;

    // WM_NCCREATE this 
    if (msg == WM_NCCREATE)
    {
        const CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->HandleMessage(hwnd, msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KILLFOCUS:
        if (m_input) m_input->Clear(); // Ŀ    ʱȭ
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (m_input)
        {
            const bool isRepeat = (lParam & (1 << 30)) != 0;
            const uint32_t vk = NormalizeNumpadVK((uint32_t)wParam, lParam);
            m_input->OnKeyDown(vk, isRepeat);
        }
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        {
            const uint32_t vk = NormalizeNumpadVK((uint32_t)wParam, lParam);
            m_input->OnKeyUp(vk);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (m_input)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            m_input->OnMouseMove(x, y);
        }
        return 0;

    case WM_LBUTTONDOWN: if (m_input) m_input->OnMouseButtonDown(0); return 0;
    case WM_LBUTTONUP:   if (m_input) m_input->OnMouseButtonUp(0);   return 0;
    case WM_SIZE:
    {
        // ּȭ  Ƿ 0  
        const uint32_t w = (uint32_t)LOWORD(lParam);
        const uint32_t h = (uint32_t)HIWORD(lParam);
        if (w != 0 && h != 0)
        {
            m_width = w;
            m_height = h;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}