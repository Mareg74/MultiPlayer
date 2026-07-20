#include <cstdio>
#include <vector>
#include <windows.h>
#include "SpoutLibrary.h"

int main()
{
    HMODULE mod = LoadLibraryW(L"SpoutLibrary.dll");
    if (!mod) {
        std::printf("FAIL LoadLibrary err=%lu\n", GetLastError());
        return 1;
    }
    auto getSpout = reinterpret_cast<SPOUTHANDLE(WINAPI *)(void)>(GetProcAddress(mod, "GetSpout"));
    SPOUTLIBRARY *lib = getSpout ? getSpout() : nullptr;
    if (!lib) {
        std::printf("FAIL GetSpout\n");
        return 2;
    }

    lib->SetCPUshare(true);
    lib->SetCPUmode(true);
    if (!lib->OpenDirectX11()) {
        std::printf("FAIL OpenDirectX11\n");
        lib->Release();
        return 3;
    }

    lib->SetSenderName("MultiPlayer");
    const unsigned w = 1920, h = 1080;
    std::vector<unsigned char> px(size_t(w) * h * 4, 0);
    for (unsigned i = 0; i < w * h; ++i) {
        px[i * 4 + 1] = 255;
        px[i * 4 + 3] = 255;
    }

    int okFrames = 0;
    for (int i = 0; i < 30; ++i) {
        if (lib->SendImage(px.data(), w, h, GL_RGBA, false))
            ++okFrames;
        Sleep(16);
    }

    const bool found = lib->FindSenderName("MultiPlayer");
    std::printf("SendImage ok=%d/30 init=%d cpu=%d count=%d foundSelf=%d name=%s %ux%u\n",
                okFrames, lib->IsInitialized() ? 1 : 0, lib->GetCPU() ? 1 : 0,
                lib->GetSenderCount(), found ? 1 : 0,
                lib->GetName() ? lib->GetName() : "(null)", lib->GetWidth(), lib->GetHeight());

    lib->ReleaseSender();
    lib->CloseDirectX11();
    lib->Release();
    FreeLibrary(mod);
    return (okFrames > 0 && found) ? 0 : 4;
}
