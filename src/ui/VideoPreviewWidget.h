#pragma once

#include <QWidget>
#include <QImage>

class VideoPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget *parent = nullptr);
    void setFrame(const QImage &frame);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
};
