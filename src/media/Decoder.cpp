#include "media/Decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <QMetaObject>
#include <QString>
#include <chrono>
#include <climits>

namespace {

AVPixelFormat mapHwFormat(AVCodecContext *ctx, const AVPixelFormat *pixFmts)
{
    Q_UNUSED(ctx);
    for (const AVPixelFormat *p = pixFmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == AV_PIX_FMT_VIDEOTOOLBOX || *p == AV_PIX_FMT_D3D11 || *p == AV_PIX_FMT_DXVA2_VLD
            || *p == AV_PIX_FMT_CUDA)
            return *p;
    }
    return pixFmts[0];
}

bool frameNeedsHwTransfer(const AVFrame *frame)
{
    if (!frame)
        return false;
    if (frame->hw_frames_ctx)
        return true;
    switch (frame->format) {
    case AV_PIX_FMT_VIDEOTOOLBOX:
    case AV_PIX_FMT_D3D11:
    case AV_PIX_FMT_D3D11VA_VLD:
    case AV_PIX_FMT_DXVA2_VLD:
    case AV_PIX_FMT_CUDA:
    case AV_PIX_FMT_QSV:
    case AV_PIX_FMT_VAAPI:
        return true;
    default:
        return false;
    }
}

} // namespace

Decoder::Decoder(QObject *parent)
    : QObject(parent)
{
}

Decoder::~Decoder()
{
    close();
}

bool Decoder::open(const QString &path)
{
    close();
    if (!openInternal(path)) {
        closeInternal();
        return false;
    }

    m_stopRequested = false;
    m_playing = false;
    m_open = true;

    // Decode first frame on the open thread so previews work immediately
    seekInternal(0);
    if (decodeOneFrame()) {
        emit frameReady();
        emit positionChanged(m_positionMs.load(), m_durationMs);
    }

    m_thread = std::thread([this]() { decodeLoop(); });
    emit opened(m_path, m_durationMs, m_width, m_height);
    return true;
}

void Decoder::close()
{
    m_stopRequested = true;
    m_playing = false;
    m_forceDecode = false;
    m_forceDecodeTries = 0;
    m_seekRequested = false;
    if (m_thread.joinable())
        m_thread.join();
    closeInternal();
    m_open = false;
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_latestFrame = QImage();
        m_hasFrame = false;
    }
}

void Decoder::play()
{
    if (m_open) {
        m_forceDecode = false;
        m_playing = true;
    }
}

void Decoder::pause()
{
    m_playing = false;
}

void Decoder::stop()
{
    // 1er Stop : coupe la lecture en gardant la position (preview inchangée)
    // 2e Stop (déjà arrêté) : revient au début + rafraîchit la preview
    if (m_playing) {
        m_playing = false;
    } else {
        seekMs(0);
    }
}

void Decoder::seekMs(qint64 ms)
{
    m_seekTargetMs = qMax(qint64(0), ms);
    m_seekRequested = true;
    // Scrub / rewind while paused: decode one frame at the new position
    if (!m_playing) {
        m_forceDecodeTries = 0;
        m_forceDecode = true;
    }
}

void Decoder::setLoop(bool loop)
{
    m_loop = loop;
}

bool Decoder::takeFrame(QImage &out)
{
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (!m_hasFrame)
        return false;
    out = m_latestFrame;
    return true;
}

bool Decoder::openInternal(const QString &path)
{
    m_path = path;
    const QByteArray utf8 = path.toUtf8();

    if (avformat_open_input(&m_format, utf8.constData(), nullptr, nullptr) < 0) {
        emit errorOccurred(tr("Impossible d'ouvrir le fichier"));
        return false;
    }
    if (avformat_find_stream_info(m_format, nullptr) < 0) {
        emit errorOccurred(tr("Impossible de lire les flux"));
        return false;
    }

    m_videoStream = av_find_best_stream(m_format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStream < 0) {
        emit errorOccurred(tr("Aucun flux vidéo"));
        return false;
    }

    AVStream *stream = m_format->streams[m_videoStream];
    const AVCodecID codecId = stream->codecpar->codec_id;
    const char *tag = nullptr;
    char tagBuf[8] = {};
    if (stream->codecpar->codec_tag) {
        av_fourcc_make_string(tagBuf, stream->codecpar->codec_tag);
        tag = tagBuf;
    }

    if (tag) {
        const QString t = QString::fromLatin1(tag);
        if (t.contains(QStringLiteral("DXV"), Qt::CaseInsensitive)) {
            emit errorOccurred(
                tr("Codec DXV/DXV3 non supporté (propriétaire Resolume). "
                   "Exportez en HAP, ProRes, H.264 ou H.265."));
            return false;
        }
    }

    const AVCodec *codec = avcodec_find_decoder(codecId);
    if (!codec) {
        static const char *kHapNames[] = {"hap", "hapq", "hapalpha", "hapalphahq", nullptr};
        for (int i = 0; kHapNames[i]; ++i) {
            codec = avcodec_find_decoder_by_name(kHapNames[i]);
            if (codec)
                break;
        }
    }
    if (!codec) {
        const QString name = tag ? QString::fromLatin1(tag) : QString::number(static_cast<int>(codecId));
        emit errorOccurred(tr("Codec non supporté (%1). Essayez HAP / ProRes / H.264 / H.265.").arg(name));
        return false;
    }

    m_codec = avcodec_alloc_context3(codec);
    if (!m_codec || avcodec_parameters_to_context(m_codec, stream->codecpar) < 0) {
        emit errorOccurred(tr("Échec init codec"));
        return false;
    }

    const bool isHap = QString::fromLatin1(codec->name).startsWith(QStringLiteral("hap"));
    const bool tryHw = !isHap;

    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
#if defined(_WIN32)
    hwType = AV_HWDEVICE_TYPE_D3D11VA;
#elif defined(__APPLE__)
    hwType = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#endif
    if (tryHw && hwType != AV_HWDEVICE_TYPE_NONE
        && av_hwdevice_ctx_create(&m_hwDevice, hwType, nullptr, nullptr, 0) == 0) {
        m_codec->hw_device_ctx = av_buffer_ref(m_hwDevice);
        m_codec->get_format = mapHwFormat;
        m_useHw = true;
    }

    if (avcodec_open2(m_codec, codec, nullptr) < 0) {
        if (m_useHw) {
            av_buffer_unref(&m_codec->hw_device_ctx);
            av_buffer_unref(&m_hwDevice);
            m_useHw = false;
            if (avcodec_open2(m_codec, codec, nullptr) < 0) {
                emit errorOccurred(tr("Échec ouverture codec"));
                return false;
            }
        } else {
            emit errorOccurred(tr("Échec ouverture codec"));
            return false;
        }
    }

    m_width = m_codec->width;
    m_height = m_codec->height;
    if (stream->duration > 0) {
        m_durationMs = static_cast<qint64>(stream->duration * av_q2d(stream->time_base) * 1000.0);
    } else if (m_format->duration > 0) {
        m_durationMs = m_format->duration / (AV_TIME_BASE / 1000);
    }

    m_frame = av_frame_alloc();
    m_frameRgb = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_frameRgb || !m_packet) {
        emit errorOccurred(tr("Allocation frame échouée"));
        return false;
    }

    const int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_width, m_height, 1);
    uint8_t *buf = static_cast<uint8_t *>(av_malloc(bufSize));
    av_image_fill_arrays(m_frameRgb->data, m_frameRgb->linesize, buf, AV_PIX_FMT_RGBA, m_width,
                         m_height, 1);

    return true;
}

void Decoder::closeInternal()
{
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_frameRgb) {
        av_freep(&m_frameRgb->data[0]);
        av_frame_free(&m_frameRgb);
    }
    if (m_frame)
        av_frame_free(&m_frame);
    if (m_packet)
        av_packet_free(&m_packet);
    if (m_codec) {
        avcodec_free_context(&m_codec);
    }
    if (m_hwDevice)
        av_buffer_unref(&m_hwDevice);
    if (m_format) {
        avformat_close_input(&m_format);
    }
    m_videoStream = -1;
    m_width = 0;
    m_height = 0;
    m_durationMs = 0;
    m_positionMs = 0;
    m_useHw = false;
}

void Decoder::seekInternal(qint64 targetMs)
{
    if (!m_format || m_videoStream < 0 || !m_codec)
        return;

    AVStream *stream = m_format->streams[m_videoStream];
    int64_t ts = 0;
    if (targetMs > 0)
        ts = static_cast<int64_t>((targetMs / 1000.0) / av_q2d(stream->time_base));
    else if (stream->start_time != AV_NOPTS_VALUE)
        ts = stream->start_time;

    if (av_seek_frame(m_format, m_videoStream, ts, AVSEEK_FLAG_BACKWARD) < 0)
        avformat_seek_file(m_format, m_videoStream, INT64_MIN, ts, INT64_MAX, 0);
    avcodec_flush_buffers(m_codec);
    m_positionMs = targetMs;
}

bool Decoder::decodeOneFrame()
{
    // Read/decode until one video frame is stored (used for scrub + open preview)
    for (int attempt = 0; attempt < 500 && !m_stopRequested; ++attempt) {
        const int readRet = av_read_frame(m_format, m_packet);
        if (readRet < 0) {
            // Flush decoder
            avcodec_send_packet(m_codec, nullptr);
            while (true) {
                const int ret = avcodec_receive_frame(m_codec, m_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN) || ret < 0)
                    break;
                if (transferAndStore(m_frame))
                    return true;
            }
            return false;
        }

        if (m_packet->stream_index != m_videoStream) {
            av_packet_unref(m_packet);
            continue;
        }

        if (avcodec_send_packet(m_codec, m_packet) < 0) {
            av_packet_unref(m_packet);
            continue;
        }
        av_packet_unref(m_packet);

        while (true) {
            const int ret = avcodec_receive_frame(m_codec, m_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                return false;
            if (transferAndStore(m_frame))
                return true;
        }
    }
    return false;
}

bool Decoder::transferAndStore(AVFrame *frame)
{
    AVFrame *src = frame;
    AVFrame *swFrame = nullptr;
    if (frameNeedsHwTransfer(frame)) {
        swFrame = av_frame_alloc();
        if (!swFrame || av_hwframe_transfer_data(swFrame, frame, 0) < 0) {
            if (swFrame)
                av_frame_free(&swFrame);
            return false;
        }
        src = swFrame;
    }

    const bool ok = storeFrame(src);
    if (swFrame)
        av_frame_free(&swFrame);
    if (!ok)
        return false;

    AVStream *stream = m_format->streams[m_videoStream];
    if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
        m_positionMs = static_cast<qint64>(
            frame->best_effort_timestamp * av_q2d(stream->time_base) * 1000.0);
    }
    return true;
}

void Decoder::emitFrameSignals()
{
    QMetaObject::invokeMethod(
        this,
        [this]() {
            emit frameReady();
            emit positionChanged(m_positionMs.load(), m_durationMs);
        },
        Qt::QueuedConnection);
}

void Decoder::decodeLoop()
{
    using clock = std::chrono::steady_clock;
    auto nextFrameAt = clock::now();

    while (!m_stopRequested) {
        if (m_seekRequested.exchange(false))
            seekInternal(m_seekTargetMs.load());

        const bool forceDecode = m_forceDecode.load();
        if (!m_playing && !forceDecode) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (!decodeOneFrame()) {
            if (forceDecode) {
                const int tries = ++m_forceDecodeTries;
                if (tries > 8) {
                    m_forceDecode = false;
                    m_forceDecodeTries = 0;
                } else {
                    // Re-seek to the requested position and try again
                    m_seekRequested = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                continue;
            }
            if (m_loop) {
                m_seekTargetMs = 0;
                m_seekRequested = true;
                continue;
            }
            m_playing = false;
            QMetaObject::invokeMethod(this, [this]() { emit ended(); }, Qt::QueuedConnection);
            continue;
        }

        emitFrameSignals();

        if (m_forceDecode.exchange(false)) {
            m_forceDecodeTries = 0;
            continue;
        }

        AVStream *stream = m_format->streams[m_videoStream];
        double fps = av_q2d(stream->avg_frame_rate);
        if (fps <= 1.0)
            fps = 25.0;
        nextFrameAt += std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<double>(1.0 / fps));
        std::this_thread::sleep_until(nextFrameAt);
        if (clock::now() > nextFrameAt + std::chrono::milliseconds(200))
            nextFrameAt = clock::now();
    }
}

bool Decoder::storeFrame(AVFrame *frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0)
        return false;

    const AVPixelFormat srcFmt = static_cast<AVPixelFormat>(frame->format);
    if (srcFmt == AV_PIX_FMT_NONE || av_pix_fmt_desc_get(srcFmt) == nullptr)
        return false;

    m_sws = sws_getCachedContext(m_sws, frame->width, frame->height, srcFmt, m_width, m_height,
                                 AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_sws)
        return false;

    sws_scale(m_sws, frame->data, frame->linesize, 0, frame->height, m_frameRgb->data,
              m_frameRgb->linesize);

    QImage img(m_frameRgb->data[0], m_width, m_height, m_frameRgb->linesize[0],
               QImage::Format_RGBA8888);
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_latestFrame = img.copy();
        m_hasFrame = true;
    }
    return true;
}
