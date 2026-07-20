#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct AVBufferRef;

class Decoder : public QObject
{
    Q_OBJECT

public:
    explicit Decoder(QObject *parent = nullptr);
    ~Decoder() override;

    bool open(const QString &path);
    void close();
    void play();
    void pause();
    void stop();
    void seekMs(qint64 ms);
    void setLoop(bool loop);

    bool isOpen() const { return m_open; }
    bool isPlaying() const { return m_playing; }
    bool isLooping() const { return m_loop; }
    QString path() const { return m_path; }
    qint64 durationMs() const { return m_durationMs; }
    qint64 positionMs() const { return m_positionMs.load(); }
    int width() const { return m_width; }
    int height() const { return m_height; }

    // Pull latest decoded RGBA frame (copy). Returns false if none yet.
    bool takeFrame(QImage &out);

signals:
    void opened(const QString &path, qint64 durationMs, int width, int height);
    void frameReady();
    void positionChanged(qint64 ms, qint64 durationMs);
    void ended();
    void errorOccurred(const QString &message);

private:
    void decodeLoop();
    bool openInternal(const QString &path);
    void closeInternal();
    void seekInternal(qint64 targetMs);
    bool decodeOneFrame();
    bool transferAndStore(AVFrame *frame);
    bool storeFrame(AVFrame *frame);
    void emitFrameSignals();

    QString m_path;
    std::atomic<bool> m_open{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_loop{true};
    std::atomic<bool> m_seekRequested{false};
    std::atomic<bool> m_forceDecode{false};
    std::atomic<int> m_forceDecodeTries{0};
    std::atomic<qint64> m_seekTargetMs{0};
    std::atomic<qint64> m_positionMs{0};
    qint64 m_durationMs = 0;
    int m_width = 0;
    int m_height = 0;
    int m_videoStream = -1;

    AVFormatContext *m_format = nullptr;
    AVCodecContext *m_codec = nullptr;
    AVFrame *m_frame = nullptr;
    AVFrame *m_frameRgb = nullptr;
    AVPacket *m_packet = nullptr;
    SwsContext *m_sws = nullptr;
    AVBufferRef *m_hwDevice = nullptr;
    bool m_useHw = false;

    std::thread m_thread;
    std::mutex m_frameMutex;
    QImage m_latestFrame;
    bool m_hasFrame = false;
};
