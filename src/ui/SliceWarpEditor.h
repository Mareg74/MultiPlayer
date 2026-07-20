#pragma once

#include "map/OutputMap.h"

#include <QWidget>

class SliceWarpEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SliceWarpEditor(QWidget *parent = nullptr);

    void setOutputSize(int w, int h);
    void setSlice(OutputSlice *slice); // non-owning; null clears
    void refresh();

signals:
    void sliceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF viewRect() const;
    QPointF toWidget(const QPointF &outputPt) const;
    QPointF toOutput(const QPointF &widgetPt) const;
    int hitCorner(const QPointF &widgetPos) const;

    OutputSlice *m_slice = nullptr;
    int m_outW = 1920;
    int m_outH = 1080;
    int m_dragCorner = -1;
};
