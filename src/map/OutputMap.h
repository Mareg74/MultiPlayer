#pragma once

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

struct OutputSlice
{
    QString name;
    QRectF inputRect;
    QRectF outputRect; // axis-aligned fallback / bounding
    // Destination warp corners in output pixels: TL, TR, BR, BL
    QPointF corners[4];
    bool useWarp = false;
    bool enabled = true;

    // Soft-edge feather as fraction of slice size [0..0.5]
    float softLeft = 0.f;
    float softRight = 0.f;
    float softTop = 0.f;
    float softBottom = 0.f;

    void syncCornersFromRect();
    void syncRectFromCorners();
};

class OutputMap
{
public:
    void clear();
    void setOutputSize(int width, int height);
    QSize outputSize() const { return m_outputSize; }

    QVector<OutputSlice> &slices() { return m_slices; }
    const QVector<OutputSlice> &slices() const { return m_slices; }

    void addSlice(const OutputSlice &slice);
    void resetFullscreen(int canvasW, int canvasH);

    // Keep output size + fullscreen slice aligned with composition canvas.
    void matchCanvas(int canvasW, int canvasH);

    // True when apply() would yield the same pixels as the composition (1:1).
    bool isPassthrough(QSize canvasSize) const;

    bool hasResolumeImport() const { return m_resolumeImported; }
    void setResolumeImported(bool imported) { m_resolumeImported = imported; }

    QImage apply(const QImage &composed) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    static QImage extractWithSoftEdge(const QImage &composed, const OutputSlice &s);
    static void drawWarped(QPainter *p, const QImage &src, const OutputSlice &s);

    QSize m_outputSize{1920, 1080};
    QVector<OutputSlice> m_slices;
    bool m_resolumeImported = false;
};
