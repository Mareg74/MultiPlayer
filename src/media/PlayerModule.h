#pragma once

#include "compose/ClipTransform.h"
#include "compose/OutputCanvas.h"
#include "media/Decoder.h"

#include <QObject>
#include <QString>
#include <memory>

class PlayerModule : public QObject
{
    Q_OBJECT

public:
    explicit PlayerModule(int index, QObject *parent = nullptr);

    int index() const { return m_index; }
    void setIndex(int index);
    static QString defaultNameForIndex(int index);

    QString name() const { return m_name; }
    void setName(const QString &name);

    Decoder *decoder() const { return m_decoder.get(); }
    ClipTransform &transform() { return m_transform; }
    const ClipTransform &transform() const { return m_transform; }
    QString lastError() const { return m_lastError; }

    // Empty name = no slice link
    QString linkedSliceName() const { return m_linkedSliceName; }
    void setLinkedSliceName(const QString &name) { m_linkedSliceName = name; }
    SliceAlignMode sliceAlignMode() const { return m_sliceAlignMode; }
    void setSliceAlignMode(SliceAlignMode mode) { m_sliceAlignMode = mode; }
    bool maskToSlice() const { return m_maskToSlice; }
    void setMaskToSlice(bool on) { m_maskToSlice = on; }

    bool load(const QString &path);
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    QImage currentFrame() const;

signals:
    void stateChanged();
    void frameUpdated();
    void positionChanged(qint64 ms, qint64 durationMs);
    void errorOccurred(const QString &message);

private:
    int m_index = 0;
    QString m_name;
    std::unique_ptr<Decoder> m_decoder;
    ClipTransform m_transform;
    mutable QImage m_cachedFrame;
    QString m_lastError;
    QString m_linkedSliceName;
    SliceAlignMode m_sliceAlignMode = SliceAlignMode::Fit;
    bool m_maskToSlice = true;
};
