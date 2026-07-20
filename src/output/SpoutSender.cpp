#include "output/SpoutSender.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <cstring>

#if MP_HAS_SPOUT
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "SpoutLibrary.h"

namespace {

HMODULE g_spoutModule = nullptr;
using GetSpoutFn = SPOUTHANDLE(WINAPI *)(void);
GetSpoutFn g_getSpout = nullptr;

QString spoutDllCandidates()
{
    QStringList paths;
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty())
        paths << QDir(appDir).filePath(QStringLiteral("SpoutLibrary.dll"));
    paths << QStringLiteral("SpoutLibrary.dll");
    return paths.join(QLatin1Char('|'));
}

bool loadSpoutLibrary()
{
    if (g_getSpout)
        return true;

    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList tryPaths;
    if (!appDir.isEmpty())
        tryPaths << QDir(appDir).filePath(QStringLiteral("SpoutLibrary.dll"));
    tryPaths << QStringLiteral("SpoutLibrary.dll");

    for (const QString &path : tryPaths) {
        g_spoutModule = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (g_spoutModule)
            break;
    }
    if (!g_spoutModule) {
        qWarning("Spout: LoadLibrary failed (tried %s) err=%lu",
                 qPrintable(spoutDllCandidates()), GetLastError());
        return false;
    }

    g_getSpout = reinterpret_cast<GetSpoutFn>(GetProcAddress(g_spoutModule, "GetSpout"));
    if (!g_getSpout) {
        qWarning("Spout: GetProcAddress(GetSpout) failed");
        FreeLibrary(g_spoutModule);
        g_spoutModule = nullptr;
        return false;
    }
    return true;
}

// Spout SendImage assumes tightly packed rows (no QImage bytesPerLine padding).
QImage makePackedRgba(const QImage &src, int width, int height)
{
    QImage rgba = src.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.width() != width || rgba.height() != height) {
        rgba = rgba.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (rgba.format() != QImage::Format_RGBA8888)
            rgba = rgba.convertToFormat(QImage::Format_RGBA8888);
    }

    const int rowBytes = width * 4;
    if (rgba.bytesPerLine() == rowBytes)
        return rgba;

    QImage packed(width, height, QImage::Format_RGBA8888);
    packed.fill(0);
    for (int y = 0; y < height; ++y)
        std::memcpy(packed.scanLine(y), rgba.constScanLine(y), size_t(rowBytes));
    return packed;
}

} // namespace
#endif

SpoutSender::SpoutSender() = default;

SpoutSender::~SpoutSender()
{
    stop();
}

bool SpoutSender::isAvailable() const
{
#if MP_HAS_SPOUT
    return loadSpoutLibrary();
#else
    return false;
#endif
}

bool SpoutSender::start(const QString &name, int width, int height)
{
    stop();
    m_name = name;
    m_nameUtf8 = name.toUtf8();
    m_width = qMax(1, width);
    m_height = qMax(1, height);

#if MP_HAS_SPOUT
    if (!loadSpoutLibrary() || !g_getSpout)
        return false;

    SPOUTLIBRARY *lib = g_getSpout();
    if (!lib) {
        qWarning("Spout: GetSpout() returned null");
        return false;
    }

    // Prefer CPU shared-memory path: works without GL interop (VMs / multi-GPU / Qt D3D).
    lib->SetCPUshare(true);
    lib->SetCPUmode(true);

    if (!lib->OpenDirectX11()) {
        qWarning("Spout: OpenDirectX11() failed");
        lib->Release();
        return false;
    }

    m_spout = lib;
    lib->SetSenderName(m_nameUtf8.constData());
    m_running = true;
    qInfo("Spout: sender ready name=%s %dx%d cpu=%d gldx=%d", m_nameUtf8.constData(), m_width,
          m_height, lib->GetCPU() ? 1 : 0, lib->GetGLDX() ? 1 : 0);
    return true;
#else
    m_running = true;
    return true;
#endif
}

void SpoutSender::stop()
{
#if MP_HAS_SPOUT
    if (m_spout) {
        auto *lib = static_cast<SPOUTLIBRARY *>(m_spout);
        lib->ReleaseSender();
        lib->CloseDirectX11();
        lib->Release();
        m_spout = nullptr;
    }
#endif
    m_running = false;
}

bool SpoutSender::send(const QImage &frame)
{
    if (!m_running || frame.isNull())
        return false;

#if MP_HAS_SPOUT
    if (!m_spout)
        return false;

    const QImage rgba = makePackedRgba(frame, m_width, m_height);
    auto *lib = static_cast<SPOUTLIBRARY *>(m_spout);
    const bool ok = lib->SendImage(rgba.constBits(), static_cast<unsigned int>(rgba.width()),
                                   static_cast<unsigned int>(rgba.height()), GL_RGBA, false);
    if (!ok) {
        static int s_failLog = 0;
        if (s_failLog < 5) {
            qWarning("Spout: SendImage failed initialized=%d", lib->IsInitialized() ? 1 : 0);
            ++s_failLog;
        }
    }
    return ok;
#else
    Q_UNUSED(frame);
    return true;
#endif
}
