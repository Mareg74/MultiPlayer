#include "ui/ProgramCanvasWidget.h"
#include "compose/OutputCanvas.h"
#include "map/OutputMap.h"
#include "media/PlayerModule.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtMath>
#include <functional>

namespace {

struct SnapResult {
    QPointF center;
    qreal guideV = qQNaN();
    qreal guideH = qQNaN();
};

SnapResult snapCenter(qreal cx, qreal cy, qreal halfW, qreal halfH, int canvasW, int canvasH,
                      int selfIndex, const QVector<std::shared_ptr<PlayerModule>> *modules,
                      qreal threshold,
                      const std::function<bool(int, QRectF *, QPointF *)> &geom)
{
    SnapResult r{{cx, cy}, qQNaN(), qQNaN()};

    const qreal left = cx - halfW;
    const qreal right = cx + halfW;
    const qreal top = cy - halfH;
    const qreal bottom = cy + halfH;

    qreal bestX = threshold;
    qreal bestY = threshold;
    qreal snapX = cx;
    qreal snapY = cy;
    qreal guideV = qQNaN();
    qreal guideH = qQNaN();

    auto tryX = [&](qreal edgeOrCenter, qreal target, qreal newCenter) {
        const qreal d = qAbs(edgeOrCenter - target);
        if (d < bestX) {
            bestX = d;
            snapX = newCenter;
            guideV = target;
        }
    };
    auto tryY = [&](qreal edgeOrCenter, qreal target, qreal newCenter) {
        const qreal d = qAbs(edgeOrCenter - target);
        if (d < bestY) {
            bestY = d;
            snapY = newCenter;
            guideH = target;
        }
    };

    tryX(left, 0.0, halfW);
    tryX(right, canvasW, canvasW - halfW);
    tryX(cx, canvasW * 0.5, canvasW * 0.5);
    tryY(top, 0.0, halfH);
    tryY(bottom, canvasH, canvasH - halfH);
    tryY(cy, canvasH * 0.5, canvasH * 0.5);

    if (modules) {
        for (int i = 0; i < modules->size(); ++i) {
            if (i == selfIndex)
                continue;
            QRectF rect;
            QPointF oc;
            if (!geom(i, &rect, &oc))
                continue;

            tryX(left, rect.left(), rect.left() + halfW);
            tryX(left, rect.right(), rect.right() + halfW);
            tryX(right, rect.left(), rect.left() - halfW);
            tryX(right, rect.right(), rect.right() - halfW);
            tryX(cx, oc.x(), oc.x());
            tryX(left, oc.x(), oc.x() + halfW);
            tryX(right, oc.x(), oc.x() - halfW);

            tryY(top, rect.top(), rect.top() + halfH);
            tryY(top, rect.bottom(), rect.bottom() + halfH);
            tryY(bottom, rect.top(), rect.top() - halfH);
            tryY(bottom, rect.bottom(), rect.bottom() - halfH);
            tryY(cy, oc.y(), oc.y());
            tryY(top, oc.y(), oc.y() + halfH);
            tryY(bottom, oc.y(), oc.y() - halfH);
        }
    }

    r.center = {snapX, snapY};
    r.guideV = guideV;
    r.guideH = guideH;
    return r;
}

} // namespace

ProgramCanvasWidget::ProgramCanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 150);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setToolTip(QStringLiteral(
        "Drag = déplacer (snap) · Alt = sans snap · Molette = scale module · Zoom via barre"));
}

void ProgramCanvasWidget::setModules(const QVector<std::shared_ptr<PlayerModule>> *modules)
{
    m_modules = modules;
}

void ProgramCanvasWidget::setCanvasSize(int width, int height)
{
    m_canvasW = qMax(1, width);
    m_canvasH = qMax(1, height);
    updatePreferredSize();
    update();
}

void ProgramCanvasWidget::setSelectedIndex(int index)
{
    m_selected = index;
    update();
}

void ProgramCanvasWidget::setFrame(const QImage &composed)
{
    m_frame = composed;
    update();
}

void ProgramCanvasWidget::setInputGuide(const QImage &guide, float opacity, bool visible,
                                        bool foreground)
{
    m_inputGuide = guide;
    m_guideOpacity = qBound(0.f, opacity, 1.f);
    m_guideVisible = visible && !guide.isNull();
    m_guideForeground = foreground;
    update();
}

void ProgramCanvasWidget::clearInputGuide()
{
    m_inputGuide = QImage();
    m_guideVisible = false;
    m_guideForeground = false;
    update();
}

void ProgramCanvasWidget::setInteractionLocked(bool locked)
{
    m_interactionLocked = locked;
    if (locked) {
        m_dragging = false;
        clearSnapGuides();
    }
    setCursor(locked ? Qt::ArrowCursor : Qt::ArrowCursor);
    update();
}

void ProgramCanvasWidget::setViewZoomPercent(int percent)
{
    percent = qBound(10, percent, 400);
    if (m_zoomPercent == percent && m_zoomPreset == ZoomPreset::Fit)
        return;
    m_zoomPercent = percent;
    m_zoomPreset = ZoomPreset::Fit;
    updatePreferredSize();
    emit viewZoomChanged(m_zoomPercent);
    update();
}

void ProgramCanvasWidget::applyZoomPreset(ZoomPreset preset)
{
    m_zoomPreset = preset;
    if (preset == ZoomPreset::Fit) {
        m_zoomPercent = 100;
    } else {
        // 1:1 and Taille réelle → 1 canvas pixel = 1 widget pixel
        const QSizeF fit = fitSize();
        if (fit.width() > 1.0)
            m_zoomPercent = qBound(10, qRound(100.0 * static_cast<qreal>(m_canvasW) / fit.width()),
                                   400);
        else
            m_zoomPercent = 100;
    }
    updatePreferredSize();
    emit viewZoomChanged(m_zoomPercent);
    update();
}

void ProgramCanvasWidget::clearSnapGuides()
{
    m_guideV = qQNaN();
    m_guideH = qQNaN();
}

QSizeF ProgramCanvasWidget::fitSize() const
{
    if (m_canvasW <= 0 || m_canvasH <= 0)
        return size();
    // Use current widget size as viewport; fall back to minimum when not laid out yet
    QSize viewport = size();
    if (viewport.width() < 32 || viewport.height() < 32)
        viewport = QSize(qMax(320, minimumWidth()), qMax(180, minimumHeight()));
    return QSizeF(m_canvasW, m_canvasH).scaled(viewport, Qt::KeepAspectRatio);
}

QSizeF ProgramCanvasWidget::targetImageSize() const
{
    const QSizeF fit = fitSize();
    return fit * (m_zoomPercent / 100.0);
}

void ProgramCanvasWidget::updatePreferredSize()
{
    const QSizeF target = targetImageSize();
    const int w = qMax(200, qCeil(target.width()) + 8);
    const int h = qMax(150, qCeil(target.height()) + 8);
    // Only grow minimum when zoomed past fit so the outer scroll area can scroll
    if (m_zoomPercent > 100)
        setMinimumSize(w, h);
    else
        setMinimumSize(200, 150);
}

QRectF ProgramCanvasWidget::imageRect() const
{
    const QSizeF target = targetImageSize();
    return QRectF((width() - target.width()) * 0.5, (height() - target.height()) * 0.5,
                  target.width(), target.height());
}

void ProgramCanvasWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_zoomPreset == ZoomPreset::OneToOne || m_zoomPreset == ZoomPreset::ActualSize)
        return;
    updatePreferredSize();
}

QPointF ProgramCanvasWidget::widgetToCanvas(const QPointF &widgetPos) const
{
    const QRectF r = imageRect();
    if (r.width() <= 0 || r.height() <= 0)
        return {};
    return {(widgetPos.x() - r.left()) / r.width() * m_canvasW,
            (widgetPos.y() - r.top()) / r.height() * m_canvasH};
}

QPointF ProgramCanvasWidget::canvasToWidget(const QPointF &canvasPos) const
{
    const QRectF r = imageRect();
    return {r.left() + canvasPos.x() / m_canvasW * r.width(),
            r.top() + canvasPos.y() / m_canvasH * r.height()};
}

bool ProgramCanvasWidget::moduleGeom(int index, ModuleGeom *out) const
{
    if (!m_modules || !out || index < 0 || index >= m_modules->size())
        return false;
    const auto &mod = m_modules->at(index);
    if (!mod || !mod->transform().visible)
        return false;
    const QImage frame = mod->currentFrame();
    if (frame.isNull())
        return false;
    const auto &t = mod->transform();
    const QRectF dst = OutputCanvas::destRect(frame, t, QSize(m_canvasW, m_canvasH));
    if (dst.isEmpty())
        return false;
    QRectF visible = dst;
    if (m_outputMap && mod->maskToSlice() && !mod->linkedSliceName().isEmpty()) {
        const int si = OutputCanvas::findSliceIndexByName(m_outputMap, mod->linkedSliceName());
        if (si >= 0) {
            const QRectF clipR = m_outputMap->slices()[si].inputRect;
            if (!clipR.isEmpty())
                visible = dst.intersected(clipR);
        }
    }
    if (visible.isEmpty())
        return false;
    out->rect = visible;
    out->center = QPointF(t.posX, t.posY);
    return true;
}

int ProgramCanvasWidget::hitTest(const QPointF &canvasPos) const
{
    if (!m_modules)
        return -1;
    for (int i = m_modules->size() - 1; i >= 0; --i) {
        ModuleGeom g;
        if (moduleGeom(i, &g) && g.rect.contains(canvasPos))
            return i;
    }
    return -1;
}

QPointF ProgramCanvasWidget::snapPosition(int index, QPointF center, qreal halfW, qreal halfH) const
{
    auto geomFn = [this](int i, QRectF *rect, QPointF *c) -> bool {
        ModuleGeom g;
        if (!moduleGeom(i, &g))
            return false;
        *rect = g.rect;
        *c = g.center;
        return true;
    };
    const qreal thr = qMax(kSnapThreshold, qMin(m_canvasW, m_canvasH) * 0.008);
    const SnapResult r = snapCenter(center.x(), center.y(), halfW, halfH, m_canvasW, m_canvasH,
                                    index, m_modules, thr, geomFn);
    auto *self = const_cast<ProgramCanvasWidget *>(this);
    self->m_guideV = r.guideV;
    self->m_guideH = r.guideH;
    return r.center;
}

void ProgramCanvasWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(9, 11, 14));

    const QRectF r = imageRect();
    p.fillRect(r, QColor(0, 0, 0));

    const auto drawGuide = [&]() {
        if (!m_guideVisible || m_inputGuide.isNull())
            return;
        p.save();
        p.setOpacity(static_cast<qreal>(m_guideOpacity));
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(r, m_inputGuide);
        p.restore();
    };

    if (!m_guideForeground)
        drawGuide();

    if (!m_frame.isNull()) {
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(r, m_frame);
        p.restore();
    }

    if (m_guideForeground)
        drawGuide();

    p.setPen(QPen(m_interactionLocked ? QColor(180, 70, 70) : QColor(42, 49, 60), 1));
    p.drawRect(r);

    if (m_dragging) {
        p.setPen(QPen(QColor(255, 196, 64, 210), 1, Qt::DashLine));
        if (!qIsNaN(m_guideV)) {
            p.drawLine(canvasToWidget(QPointF(m_guideV, 0)),
                       canvasToWidget(QPointF(m_guideV, m_canvasH)));
        }
        if (!qIsNaN(m_guideH)) {
            p.drawLine(canvasToWidget(QPointF(0, m_guideH)),
                       canvasToWidget(QPointF(m_canvasW, m_guideH)));
        }
    }

    if (!m_modules)
        return;

    for (int i = 0; i < m_modules->size(); ++i) {
        ModuleGeom g;
        if (!moduleGeom(i, &g))
            continue;
        const QRectF destWidget(canvasToWidget(g.rect.topLeft()),
                                canvasToWidget(g.rect.bottomRight()));
        const bool sel = (i == m_selected);
        p.setPen(QPen(sel ? QColor(61, 139, 253) : QColor(255, 255, 255, 60), sel ? 2 : 1));
        p.drawRect(destWidget);
        if (sel) {
            p.setPen(QColor(61, 139, 253));
            const auto &mod = m_modules->at(i);
            const QString label = mod ? mod->name() : PlayerModule::defaultNameForIndex(i);
            p.drawText(destWidget.adjusted(4, 2, -4, -2), label);
        }
    }
}

void ProgramCanvasWidget::mousePressEvent(QMouseEvent *event)
{
    const QPointF canvas = widgetToCanvas(event->position());
    const int hit = hitTest(canvas);
    if (hit >= 0) {
        m_selected = hit;
        emit moduleSelected(hit);
        if (!m_interactionLocked) {
            const auto &t = m_modules->at(hit)->transform();
            m_dragOffset = QPointF(canvas.x() - t.posX, canvas.y() - t.posY);
            m_dragging = true;
            clearSnapGuides();
        }
        update();
    } else {
        // Clic dans le vide → désélection
        m_selected = -1;
        m_dragging = false;
        clearSnapGuides();
        emit moduleSelected(-1);
        update();
    }
    QWidget::mousePressEvent(event);
}

void ProgramCanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_interactionLocked) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_dragging && m_modules && m_selected >= 0 && m_selected < m_modules->size()) {
        const QPointF canvas = widgetToCanvas(event->position());
        auto &t = m_modules->at(m_selected)->transform();

        QPointF center(canvas.x() - m_dragOffset.x(), canvas.y() - m_dragOffset.y());

        const float oldX = t.posX;
        const float oldY = t.posY;
        t.posX = static_cast<float>(center.x());
        t.posY = static_cast<float>(center.y());
        ModuleGeom g;
        qreal halfW = 0;
        qreal halfH = 0;
        if (moduleGeom(m_selected, &g)) {
            halfW = g.rect.width() * 0.5;
            halfH = g.rect.height() * 0.5;
        }
        t.posX = oldX;
        t.posY = oldY;

        const bool disableSnap = event->modifiers() & Qt::AltModifier;
        if (!disableSnap && halfW > 0 && halfH > 0)
            center = snapPosition(m_selected, center, halfW, halfH);
        else
            clearSnapGuides();

        t.posX = static_cast<float>(center.x());
        t.posY = static_cast<float>(center.y());
        emit moduleMoved(m_selected);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void ProgramCanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    clearSnapGuides();
    update();
    QWidget::mouseReleaseEvent(event);
}

void ProgramCanvasWidget::wheelEvent(QWheelEvent *event)
{
    if (m_interactionLocked)
        return;
    if (!m_modules || m_selected < 0 || m_selected >= m_modules->size())
        return;
    auto &t = m_modules->at(m_selected)->transform();
    const float delta = event->angleDelta().y() > 0 ? 1.05f : 0.95f;
    t.scale = qBound(0.01f, t.scale * delta, 20.f);
    t.scaleY = qBound(0.01f, t.scaleY * delta, 20.f);
    emit moduleMoved(m_selected);
    update();
}
