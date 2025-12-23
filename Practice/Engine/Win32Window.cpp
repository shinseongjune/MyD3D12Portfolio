#include "Win32Window.h"

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
    wc.lpszMenuName = nullptr;         // 메뉴 없음(템플릿 제거)
    wc.lpszClassName = m_className;
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    // 이미 등록돼있을 수도 있으니 실패를 엄격히 보지 말고 확인
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

    // 클라이언트 영역이 width/height가 되도록 전체 창 크기 보정
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
        this // ★ this를 전달해서 WndProc에서 연결
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

    // WM_NCCREATE에서 this를 저장
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
    case WM_SIZE:
    {
        // 최소화 등에서도 들어오므로 0일 수 있음
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
