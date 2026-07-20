#pragma once

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QWidget>
#include <QVector>
#include <memory>

class PlayerModule;
class OutputMap;

class ProgramCanvasWidget : public QWidget
{
    Q_OBJECT

public:
    enum class ZoomPreset { Fit, OneToOne, ActualSize };

    explicit ProgramCanvasWidget(QWidget *parent = nullptr);

    void setModules(const QVector<std::shared_ptr<PlayerModule>> *modules);
    void setOutputMap(const OutputMap *map) { m_outputMap = map; }
    void setCanvasSize(int width, int height);
    void setSelectedIndex(int index);
    void setFrame(const QImage &composed);
    void setInputGuide(const QImage &guide, float opacity, bool visible, bool foreground = false);
    void clearInputGuide();

    void setInteractionLocked(bool locked);

    // View zoom: percent relative to Fit (100 = fit). Absolute 1:1 uses screen pixels.
    void setViewZoomPercent(int percent);
    int viewZoomPercent() const { return m_zoomPercent; }
    void applyZoomPreset(ZoomPreset preset);
    ZoomPreset zoomPreset() const { return m_zoomPreset; }

signals:
    void moduleSelected(int index); // -1 = none
    void moduleMoved(int index);
    void viewZoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct ModuleGeom {
        QRectF rect;
        QPointF center;
    };

    QRectF imageRect() const;
    QSizeF fitSize() const;
    QSizeF targetImageSize() const;
    void updatePreferredSize();
    QPointF widgetToCanvas(const QPointF &widgetPos) const;
    QPointF canvasToWidget(const QPointF &canvasPos) const;
    int hitTest(const QPointF &canvasPos) const;
    bool moduleGeom(int index, ModuleGeom *out) const;
    QPointF snapPosition(int index, QPointF center, qreal halfW, qreal halfH) const;
    void clearSnapGuides();

    const QVector<std::shared_ptr<PlayerModule>> *m_modules = nullptr;
    const OutputMap *m_outputMap = nullptr;
    QImage m_frame;
    QImage m_inputGuide;
    float m_guideOpacity = 0.45f;
    bool m_guideVisible = false;
    bool m_guideForeground = false;
    int m_canvasW = 1920;
    int m_canvasH = 1080;
    int m_selected = -1;
    bool m_dragging = false;
    bool m_interactionLocked = false;
    QPointF m_dragOffset;

    ZoomPreset m_zoomPreset = ZoomPreset::Fit;
    int m_zoomPercent = 100; // relative to Fit size

    qreal m_guideV = qQNaN();
    qreal m_guideH = qQNaN();
    static constexpr qreal kSnapThreshold = 18.0;
};
