#include "ui/SliceWarpEditor.h"

#include <QMouseEvent>
#include <QPainter>

SliceWarpEditor::SliceWarpEditor(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setMouseTracking(true);
}

void SliceWarpEditor::setOutputSize(int w, int h)
{
    m_outW = qMax(1, w);
    m_outH = qMax(1, h);
    update();
}

void SliceWarpEditor::setSlice(OutputSlice *slice)
{
    m_slice = slice;
    update();
}

void SliceWarpEditor::refresh()
{
    update();
}

QRectF SliceWarpEditor::viewRect() const
{
    const QSizeF target = QSizeF(m_outW, m_outH).scaled(size(), Qt::KeepAspectRatio);
    return QRectF((width() - target.width()) * 0.5, (height() - target.height()) * 0.5,
                  target.width(), target.height());
}

QPointF SliceWarpEditor::toWidget(const QPointF &outputPt) const
{
    const QRectF r = viewRect();
    return {r.left() + outputPt.x() / m_outW * r.width(),
            r.top() + outputPt.y() / m_outH * r.height()};
}

QPointF SliceWarpEditor::toOutput(const QPointF &widgetPt) const
{
    const QRectF r = viewRect();
    return {(widgetPt.x() - r.left()) / r.width() * m_outW,
            (widgetPt.y() - r.top()) / r.height() * m_outH};
}

int SliceWarpEditor::hitCorner(const QPointF &widgetPos) const
{
    if (!m_slice)
        return -1;
    for (int i = 0; i < 4; ++i) {
        const QPointF p = toWidget(m_slice->corners[i]);
        if (QLineF(p, widgetPos).length() <= 10.0)
            return i;
    }
    return -1;
}

void SliceWarpEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(12, 14, 18));
    const QRectF r = viewRect();
    p.fillRect(r, QColor(0, 0, 0));
    p.setPen(QPen(QColor(42, 49, 60), 1));
    p.drawRect(r);

    if (!m_slice)
        return;

    QPolygonF poly;
    for (int i = 0; i < 4; ++i)
        poly << toWidget(m_slice->corners[i]);

    p.setPen(QPen(m_slice->useWarp ? QColor(61, 139, 253) : QColor(120, 130, 145), 2));
    p.setBrush(QColor(61, 139, 253, 30));
    p.drawPolygon(poly);

    const QString labels[4] = {QStringLiteral("TL"), QStringLiteral("TR"), QStringLiteral("BR"),
                               QStringLiteral("BL")};
    for (int i = 0; i < 4; ++i) {
        const QPointF c = toWidget(m_slice->corners[i]);
        p.setBrush(QColor(255, 80, 80));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, 6, 6);
        p.setPen(QColor(230, 230, 230));
        p.drawText(c + QPointF(8, -4), labels[i]);
    }
}

void SliceWarpEditor::mousePressEvent(QMouseEvent *event)
{
    m_dragCorner = hitCorner(event->position());
    if (m_dragCorner >= 0 && m_slice) {
        m_slice->useWarp = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void SliceWarpEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragCorner >= 0 && m_slice) {
        QPointF out = toOutput(event->position());
        out.setX(qBound(0.0, out.x(), static_cast<qreal>(m_outW)));
        out.setY(qBound(0.0, out.y(), static_cast<qreal>(m_outH)));
        m_slice->corners[m_dragCorner] = out;
        m_slice->syncRectFromCorners();
        emit sliceChanged();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SliceWarpEditor::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragCorner = -1;
    QWidget::mouseReleaseEvent(event);
}
