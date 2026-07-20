#include "output/NdiSender.h"

#include <QtGlobal>

#if MP_HAS_NDI
#include <Processing.NDI.Lib.h>
#include <Processing.NDI.DynamicLoad.h>

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

const NDIlib_v6 *g_ndi = nullptr;
#if defined(_WIN32)
HMODULE g_ndiModule = nullptr;
#else
void *g_ndiModule = nullptr;
#endif

bool loadNdiLibrary()
{
    if (g_ndi)
        return true;

#if defined(_WIN32)
    const char *runtimeDir = std::getenv(NDILIB_REDIST_FOLDER);
    if (runtimeDir && runtimeDir[0] != '\0') {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", runtimeDir, NDILIB_LIBRARY_NAME);
        g_ndiModule = LoadLibraryA(path);
    }
    if (!g_ndiModule)
        g_ndiModule = LoadLibraryA(NDILIB_LIBRARY_NAME);
    if (!g_ndiModule)
        return false;

    using LoadFn = const NDIlib_v6 *(*)(void);
    auto loadFn = reinterpret_cast<LoadFn>(GetProcAddress(g_ndiModule, "NDIlib_v6_load"));
    if (!loadFn)
        loadFn = reinterpret_cast<LoadFn>(GetProcAddress(g_ndiModule, "NDIlib_v5_load"));
    if (!loadFn)
        return false;
    g_ndi = loadFn();
#else
    const char *runtimeDir = std::getenv(NDILIB_REDIST_FOLDER);
    if (runtimeDir && runtimeDir[0] != '\0') {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", runtimeDir, NDILIB_LIBRARY_NAME);
        g_ndiModule = dlopen(path, RTLD_LOCAL | RTLD_LAZY);
    }
    if (!g_ndiModule)
        g_ndiModule = dlopen(NDILIB_LIBRARY_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!g_ndiModule)
        g_ndiModule = dlopen("/usr/local/lib/libndi.dylib", RTLD_LOCAL | RTLD_LAZY);
    if (!g_ndiModule)
        return false;

    using LoadFn = const NDIlib_v6 *(*)(void);
    auto loadFn = reinterpret_cast<LoadFn>(dlsym(g_ndiModule, "NDIlib_v6_load"));
    if (!loadFn)
        loadFn = reinterpret_cast<LoadFn>(dlsym(g_ndiModule, "NDIlib_v5_load"));
    if (!loadFn)
        return false;
    g_ndi = loadFn();
#endif
    return g_ndi != nullptr;
}

} // namespace
#endif

NdiSender::NdiSender() = default;

NdiSender::~NdiSender()
{
    stop();
}

bool NdiSender::isAvailable() const
{
#if MP_HAS_NDI
    return loadNdiLibrary();
#else
    return false;
#endif
}

void NdiSender::setFps(int fps)
{
    m_fps = qBound(1, fps, 120);
}

void NdiSender::setSendAlpha(bool alpha)
{
    m_sendAlpha = alpha;
}

void NdiSender::setBandwidth(NdiBandwidth bandwidth)
{
    m_bandwidth = bandwidth;
}

bool NdiSender::start(const QString &name, int width, int height)
{
    stop();
    m_name = name;
    m_nameUtf8 = name.toUtf8();
    m_width = width;
    m_height = height;

#if MP_HAS_NDI
    if (!loadNdiLibrary() || !g_ndi || !g_ndi->initialize || !g_ndi->initialize())
        return false;

    NDIlib_send_create_t desc;
    desc.p_ndi_name = m_nameUtf8.constData();
    desc.p_groups = nullptr;
    desc.clock_video = true;
    desc.clock_audio = false;
    m_instance = g_ndi->send_create(&desc);
    if (!m_instance)
        return false;
    m_running = true;
    return true;
#else
    Q_UNUSED(width);
    Q_UNUSED(height);
    m_running = true;
    return true;
#endif
}

void NdiSender::stop()
{
#if MP_HAS_NDI
    if (m_instance && g_ndi && g_ndi->send_destroy) {
        g_ndi->send_destroy(static_cast<NDIlib_send_instance_t>(m_instance));
        m_instance = nullptr;
        if (g_ndi->destroy)
            g_ndi->destroy();
    }
#endif
    m_running = false;
}

bool NdiSender::send(const QImage &frame)
{
    if (!m_running || frame.isNull())
        return false;

#if MP_HAS_NDI
    if (!g_ndi || !g_ndi->send_send_video_v2 || !m_instance)
        return false;

    // On little-endian, Format_ARGB32 is stored as B,G,R,A in memory — matches
    // NDIlib_FourCC_type_BGRA. Resolume activates the alpha plane with BGRA (not RGBA).
    QImage img = frame.convertToFormat(QImage::Format_ARGB32);
    if (img.width() != m_width || img.height() != m_height) {
        img = img.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (img.format() != QImage::Format_ARGB32)
            img = img.convertToFormat(QImage::Format_ARGB32);
    }

    NDIlib_video_frame_v2_t video{};
    video.xres = img.width();
    video.yres = img.height();
    video.FourCC = m_sendAlpha ? NDIlib_FourCC_type_BGRA : NDIlib_FourCC_type_BGRX;
    video.frame_rate_N = m_fps * 1000;
    video.frame_rate_D = 1000;
    video.picture_aspect_ratio =
        static_cast<float>(img.width()) / static_cast<float>(qMax(1, img.height()));
    video.frame_format_type = NDIlib_frame_format_type_progressive;
    video.timecode = NDIlib_send_timecode_synthesize;
    video.p_data = const_cast<uint8_t *>(img.constBits());
    video.line_stride_in_bytes = static_cast<int>(img.bytesPerLine());
    video.p_metadata = nullptr;
    video.timestamp = 0;

    if (m_bandwidth == NdiBandwidth::Lowest) {
        // Hint receivers via metadata; bandwidth is primarily a receive-side concept in NDI.
        static const char *kLow = "<ndi_video_codec name=\"SpeedHQ\"/>";
        video.p_metadata = kLow;
    }

    g_ndi->send_send_video_v2(static_cast<NDIlib_send_instance_t>(m_instance), &video);
    return true;
#else
    Q_UNUSED(frame);
    return true;
#endif
}
