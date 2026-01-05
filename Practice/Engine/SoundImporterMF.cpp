#include "SoundImporterMF.h"
#include "Utilities.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmreg.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

static bool g_mfStarted = false;

static HRESULT EnsureMF()
{
    if (!g_mfStarted)
    {
        // 보통 엔진 초기화에서 CoInitializeEx도 같이 잡아둠
        HRESULT hr = MFStartup(MF_VERSION);
        if (SUCCEEDED(hr)) g_mfStarted = true;
        return hr;
    }
    return S_OK;
}

Result<SoundClip> SoundImporterMF::DecodeToPCM(const std::string& path)
{
    ThrowIfFailed(EnsureMF());

    // UTF-8 -> wchar 변환 필요
    std::wstring wpath = Utf8ToWide(path);

    ComPtr<IMFSourceReader> reader;
    ThrowIfFailed(MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader));

    // 출력 타입을 PCM으로 강제
    ComPtr<IMFMediaType> outType;
    ThrowIfFailed(MFCreateMediaType(&outType));
    ThrowIfFailed(outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
    ThrowIfFailed(outType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
    ThrowIfFailed(reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, outType.Get()));

    // 실제로 설정된 타입을 다시 얻어서 WAVEFORMATEX 구성
    ComPtr<IMFMediaType> curType;
    ThrowIfFailed(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &curType));

    UINT32 channels = 0, sampleRate = 0, bits = 0;
    ThrowIfFailed(curType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels));
    ThrowIfFailed(curType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate));
    ThrowIfFailed(curType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits));

    SoundClip clip;
    clip.wfx.wFormatTag = WAVE_FORMAT_PCM;
    clip.wfx.nChannels = (WORD)channels;
    clip.wfx.nSamplesPerSec = sampleRate;
    clip.wfx.wBitsPerSample = (WORD)bits;
    clip.wfx.nBlockAlign = (WORD)((channels * bits) / 8);
    clip.wfx.nAvgBytesPerSec = sampleRate * clip.wfx.nBlockAlign;

    // 샘플 루프: PCM 바이트를 누적
    while (true)
    {
        DWORD streamFlags = 0;
        ComPtr<IMFSample> sample;
        ThrowIfFailed(reader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, nullptr, &streamFlags, nullptr, &sample));

        if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buf;
        ThrowIfFailed(sample->ConvertToContiguousBuffer(&buf));

        BYTE* data = nullptr;
        DWORD maxLen = 0, curLen = 0;
        ThrowIfFailed(buf->Lock(&data, &maxLen, &curLen));

        size_t old = clip.pcm.size();
        clip.pcm.resize(old + curLen);
        std::memcpy(clip.pcm.data() + old, data, curLen);

        buf->Unlock();
    }

    return Result<SoundClip>(clip);
}