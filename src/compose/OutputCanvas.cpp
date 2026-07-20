#include "compose/OutputCanvas.h"
#include "map/OutputMap.h"
#include "media/PlayerModule.h"

#include <QPainter>
#include <QtMath>

void OutputCanvas::setSize(int width, int height)
{
    m_size = QSize(qMax(16, width), qMax(16, height));
}

int OutputCanvas::findSliceIndexByName(const OutputMap *map, const QString &name)
{
    if (!map || name.isEmpty())
        return -1;
    for (int i = 0; i < map->slices().size(); ++i) {
        if (map->slices()[i].name == name)
            return i;
    }
    return -1;
}

QRectF OutputCanvas::sourceRect(const QImage &src, const ClipTransform &t)
{
    const qreal w = src.width();
    const qreal h = src.height();
    const qreal left = w * qBound(0.0, static_cast<qreal>(t.cropLeft), 0.49);
    const qreal top = h * qBound(0.0, static_cast<qreal>(t.cropTop), 0.49);
    const qreal right = w * qBound(0.0, static_cast<qreal>(t.cropRight), 0.49);
    const qreal bottom = h * qBound(0.0, static_cast<qreal>(t.cropBottom), 0.49);
    return QRectF(left, top, w - left - right, h - top - bottom);
}

QRectF OutputCanvas::destRect(const QImage &src, const ClipTransform &t, const QSize &canvas)
{
    const QRectF srcR = sourceRect(src, t);
    if (srcR.width() < 1.0 || srcR.height() < 1.0 || canvas.width() < 1 || canvas.height() < 1)
        return {};

    // scale/scaleY=1 → contain cropped source inside the full composition canvas
    const qreal fit =
        qMin(static_cast<qreal>(canvas.width()) / srcR.width(),
             static_cast<qreal>(canvas.height()) / srcR.height());
    const qreal targetW = srcR.width() * fit * static_cast<qreal>(t.scale);
    const qreal targetH = srcR.height() * fit * static_cast<qreal>(t.scaleY);
    return QRectF(t.posX - targetW * 0.5, t.posY - targetH * 0.5, targetW, targetH);
}

void OutputCanvas::alignTransformToRect(ClipTransform &t, const QImage &frame, const QSize &canvas,
                                        const QRectF &sliceInput, SliceAlignMode mode)
{
    if (frame.isNull() || canvas.width() < 1 || canvas.height() < 1 || sliceInput.isEmpty())
        return;

    const QRectF srcR = sourceRect(frame, t);
    if (srcR.width() < 1.0 || srcR.height() < 1.0)
        return;

    const qreal fit =
        qMin(static_cast<qreal>(canvas.width()) / srcR.width(),
             static_cast<qreal>(canvas.height()) / srcR.height());
    const qreal fullW = srcR.width() * fit;
    const qreal fullH = srcR.height() * fit;
    if (fullW < 1.0 || fullH < 1.0)
        return;

    t.posX = static_cast<float>(sliceInput.center().x());
    t.posY = static_cast<float>(sliceInput.center().y());

    const qreal sliceW = sliceInput.width();
    const qreal sliceH = sliceInput.height();

    switch (mode) {
    case SliceAlignMode::Fit: {
        const float s = static_cast<float>(qMin(sliceW / fullW, sliceH / fullH));
        t.scale = s;
        t.scaleY = s;
        break;
    }
    case SliceAlignMode::Fill: {
        const float s = static_cast<float>(qMax(sliceW / fullW, sliceH / fullH));
        t.scale = s;
        t.scaleY = s;
        break;
    }
    case SliceAlignMode::Stretch: {
        t.scale = static_cast<float>(sliceW / fullW);
        t.scaleY = static_cast<float>(sliceH / fullH);
        break;
    }
    }
}

QImage OutputCanvas::compose(const QVector<std::shared_ptr<PlayerModule>> &modules,
                             const OutputMap *map) const
{
    // Transparent clear so NDI/Spout can carry a real alpha matte (empty = 0).
    QImage out(m_size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (const auto &mod : modules) {
        if (!mod)
            continue;
        const ClipTransform &t = mod->transform();
        if (!t.visible || t.opacity <= 0.f)
            continue;
        const QImage frame = mod->currentFrame();
        if (frame.isNull())
            continue;

        const QRectF src = sourceRect(frame, t);
        const QRectF dst = destRect(frame, t, m_size);
        if (dst.isEmpty())
            continue;

        p.save();
        p.setOpacity(qBound(0.0, static_cast<qreal>(t.opacity), 1.0));

        // Mask outside linked slice (Fill overflow, manual scale, etc.)
        if (map && mod->maskToSlice() && !mod->linkedSliceName().isEmpty()) {
            const int si = findSliceIndexByName(map, mod->linkedSliceName());
            if (si >= 0) {
                const QRectF clipR = map->slices()[si].inputRect;
                if (!clipR.isEmpty())
                    p.setClipRect(clipR);
            }
        }

        p.drawImage(dst, frame, src);
        p.restore();
    }

    p.end();
    // Straight alpha for NDI receivers (Resolume “Straight”)
    return out.convertToFormat(QImage::Format_ARGB32);
}
