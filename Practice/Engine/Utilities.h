#pragma once
#include <string>
#include <optional>
#include <Windows.h>
#include <stdexcept>

struct ImportError
{
    std::string message;
};

template<typename T>
struct Result
{
    T value{};
    std::optional<ImportError> error;

    static Result Ok(T v) { Result r; r.value = std::move(v); return r; }
    static Result Fail(std::string msg) { Result r; r.error = ImportError{ std::move(msg) }; return r; }

    bool IsOk() const { return !error.has_value(); }
};

static inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        char buf[64];
        sprintf_s(buf, "HRESULT failed: 0x%08X\n", (unsigned)hr);
        OutputDebugStringA(buf);
        throw std::runtime_error(buf);
    }
}

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}