#include "ui/OutputButton.h"

#include <QTimer>

OutputButton::OutputButton(QWidget *parent)
    : QPushButton(parent)
    , m_blinkTimer(new QTimer(this))
{
    setCheckable(false);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(200, 52);
    setText(QStringLiteral("OUTPUT"));
    connect(this, &QPushButton::clicked, this, &OutputButton::onClicked);
    connect(m_blinkTimer, &QTimer::timeout, this, &OutputButton::onBlink);
    m_blinkTimer->setInterval(420);
    updateVisual();
}

void OutputButton::setOnAir(bool onAir)
{
    if (m_onAir == onAir)
        return;
    m_onAir = onAir;
    m_blinkOn = true;
    if (m_onAir)
        m_blinkTimer->start();
    else
        m_blinkTimer->stop();
    updateVisual();
}

void OutputButton::onClicked()
{
    setOnAir(!m_onAir);
    emit outputToggled(m_onAir);
}

void OutputButton::onBlink()
{
    m_blinkOn = !m_blinkOn;
    updateVisual();
}

void OutputButton::updateVisual()
{
    if (!m_onAir) {
        setText(QStringLiteral("OUTPUT"));
        setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #9e1212;"
            "  color: #ffffff;"
            "  font-family: 'Avenir Next', 'Helvetica Neue', sans-serif;"
            "  font-weight: 800;"
            "  font-size: 16px;"
            "  letter-spacing: 2px;"
            "  border: 2px solid #5c0a0a;"
            "  border-radius: 2px;"
            "  padding: 10px 22px;"
            "}"
            "QPushButton:hover { background-color: #c41818; }"
            "QPushButton:pressed { background-color: #7a0e0e; }"));
        return;
    }

    setText(QStringLiteral("OUTPUT  ·  ON AIR"));
    const QString bg = m_blinkOn ? QStringLiteral("#ff2020") : QStringLiteral("#6e1010");
    const QString border = m_blinkOn ? QStringLiteral("#ff6a6a") : QStringLiteral("#a02020");
    setStyleSheet(QStringLiteral(
                      "QPushButton {"
                      "  background-color: %1;"
                      "  color: #ffffff;"
                      "  font-family: 'Avenir Next', 'Helvetica Neue', sans-serif;"
                      "  font-weight: 800;"
                      "  font-size: 16px;"
                      "  letter-spacing: 1.5px;"
                      "  border: 2px solid %2;"
                      "  border-radius: 2px;"
                      "  padding: 10px 22px;"
                      "}")
                      .arg(bg, border));
}
