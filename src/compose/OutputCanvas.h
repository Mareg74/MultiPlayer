#pragma once

#include "compose/ClipTransform.h"

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <memory>

class PlayerModule;
class OutputMap;

enum class SliceAlignMode {
    Fit = 0,     // contain inside slice (keep aspect)
    Fill = 1,    // cover slice (keep aspect, may crop visually)
    Stretch = 2  // match slice size exactly (may distort)
};

class OutputCanvas
{
public:
    void setSize(int width, int height);
    QSize size() const { return m_size; }

    // If map is set, modules linked to a slice with maskToSlice clip to that slice's inputRect.
    QImage compose(const QVector<std::shared_ptr<PlayerModule>> &modules,
                   const OutputMap *map = nullptr) const;

    // Cropped source region in source pixels
    static QRectF sourceRect(const QImage &src, const ClipTransform &t);

    // Destination on canvas. scale/scaleY=1 → contain cropped source in the full canvas.
    static QRectF destRect(const QImage &src, const ClipTransform &t, const QSize &canvas);

    // Place clip so its dest matches sliceInput according to mode.
    static void alignTransformToRect(ClipTransform &t, const QImage &frame, const QSize &canvas,
                                     const QRectF &sliceInput, SliceAlignMode mode);

    static int findSliceIndexByName(const OutputMap *map, const QString &name);

private:
    QSize m_size{1920, 1080};
};
