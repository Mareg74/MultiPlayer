#pragma once

#include <QPushButton>

class OutputButton : public QPushButton
{
    Q_OBJECT

public:
    explicit OutputButton(QWidget *parent = nullptr);

    bool isOnAir() const { return m_onAir; }
    void setOnAir(bool onAir);

signals:
    void outputToggled(bool onAir);

private slots:
    void onClicked();
    void onBlink();

private:
    void updateVisual();

    bool m_onAir = false;
    bool m_blinkOn = true;
    class QTimer *m_blinkTimer = nullptr;
};
