#include "ui/PlayerModuleWidget.h"
#include "ui/VideoPreviewWidget.h"
#include "media/MediaBank.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QPixmap makeLoopPixmap(const QColor &color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = size;
    const qreal penW = qMax(1.6, s * 0.11);
    QPen pen(color, penW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // Rounded cycle (classic media “repeat” silhouette)
    const QRectF oval(s * 0.22, s * 0.20, s * 0.56, s * 0.58);
    p.drawArc(oval, 55 * 16, 160 * 16);
    p.drawArc(oval, 235 * 16, 160 * 16);

    // Arrow heads
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    const qreal ah = s * 0.16;

    // Top-right arrow pointing right/down into the arc
    {
        const QPointF tip(s * 0.72, s * 0.28);
        QPolygonF tri;
        tri << tip << QPointF(tip.x() - ah, tip.y() - ah * 0.15)
            << QPointF(tip.x() - ah * 0.15, tip.y() + ah * 0.75);
        p.drawPolygon(tri);
    }
    // Bottom-left arrow pointing left/up
    {
        const QPointF tip(s * 0.28, s * 0.70);
        QPolygonF tri;
        tri << tip << QPointF(tip.x() + ah, tip.y() + ah * 0.15)
            << QPointF(tip.x() + ah * 0.15, tip.y() - ah * 0.75);
        p.drawPolygon(tri);
    }

    return pm;
}

QIcon makeLoopIcon()
{
    QIcon icon;
    icon.addPixmap(makeLoopPixmap(QColor(QStringLiteral("#8b95a8")), 32), QIcon::Normal, QIcon::Off);
    icon.addPixmap(makeLoopPixmap(QColor(QStringLiteral("#f2f6fc")), 32), QIcon::Normal, QIcon::On);
    icon.addPixmap(makeLoopPixmap(QColor(QStringLiteral("#8b95a8")), 32), QIcon::Active, QIcon::Off);
    icon.addPixmap(makeLoopPixmap(QColor(QStringLiteral("#ffffff")), 32), QIcon::Active, QIcon::On);
    return icon;
}

QPixmap makeEyePixmap(const QColor &color, int size, bool barred)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = size;
    QPen pen(color, qMax(1.5, s * 0.10), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // Almond outline
    QPainterPath almond;
    almond.moveTo(s * 0.12, s * 0.50);
    almond.cubicTo(s * 0.30, s * 0.22, s * 0.70, s * 0.22, s * 0.88, s * 0.50);
    almond.cubicTo(s * 0.70, s * 0.78, s * 0.30, s * 0.78, s * 0.12, s * 0.50);
    p.drawPath(almond);

    // Pupil
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(s * 0.50, s * 0.50), s * 0.12, s * 0.16);

    if (barred) {
        p.setPen(QPen(color, qMax(1.8, s * 0.12), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(s * 0.18, s * 0.78), QPointF(s * 0.82, s * 0.22));
    }

    return pm;
}

QIcon makeEyeIcon(bool visible)
{
    const QColor color(QStringLiteral("#c5cedd"));
    QIcon icon;
    icon.addPixmap(makeEyePixmap(color, 32, !visible), QIcon::Normal, QIcon::Off);
    icon.addPixmap(makeEyePixmap(color, 32, !visible), QIcon::Active, QIcon::Off);
    return icon;
}

} // namespace

PlayerModuleWidget::PlayerModuleWidget(int index, QWidget *parent)
    : QFrame(parent)
    , m_index(index)
{
    setObjectName(QStringLiteral("playerModule"));
    setFrameShape(QFrame::StyledPanel);
    setAcceptDrops(true);
    setToolTip(QStringLiteral("Glisser-déposer un média ici (explorateur ou banque)"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    m_nameEdit = new QLineEdit(PlayerModule::defaultNameForIndex(index), this);
    m_nameEdit->setPlaceholderText(QStringLiteral("Nom"));
    m_nameEdit->setToolTip(QStringLiteral("Nom du player (modifiable)"));
    m_nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  font-weight: 800; color: #eef2f7; font-size: 12px; letter-spacing: 0.4px;"
        "  background: transparent; border: none; padding: 0; margin: 0;"
        "  selection-background-color: #2f6fbf;"
        "}"
        "QLineEdit:hover, QLineEdit:focus {"
        "  background: #1a222d; border: 1px solid #303848; border-radius: 2px; padding: 1px 3px;"
        "}"));
    m_fileLabel = new QLabel(QStringLiteral("—"), this);
    m_fileLabel->setStyleSheet(QStringLiteral("color: #6f7a8a; font-size: 9px;"));
    m_fileLabel->setWordWrap(false);
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_fileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_fileLabel->setMinimumWidth(0);
    m_fileLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *header = new QHBoxLayout();
    header->setSpacing(6);
    header->addWidget(m_nameEdit, 1);
    m_eyeButton = new QPushButton(this);
    m_eyeButton->setCheckable(true);
    m_eyeButton->setChecked(true);
    m_eyeButton->setFixedSize(26, 22);
    m_eyeButton->setIconSize(QSize(16, 16));
    m_eyeButton->setCursor(Qt::PointingHandCursor);
    m_eyeButton->setToolTip(QStringLiteral("Afficher / masquer dans la sortie"));
    m_eyeButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  padding: 1px;"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 2px;"
        "}"
        "QPushButton:hover {"
        "  background: #1e2834;"
        "  border: 1px solid #303848;"
        "}"
        "QPushButton:!checked {"
        "  background: #1a1512;"
        "  border: 1px solid #5a4030;"
        "}"));
    updateEyeIcon();
    header->addWidget(m_eyeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    root->addLayout(header);
    root->addWidget(m_fileLabel);

    m_preview = new VideoPreviewWidget(this);
    root->addWidget(m_preview, 1);

    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setEnabled(false);
    root->addWidget(m_seekSlider);

    auto *controls = new QHBoxLayout();
    controls->setSpacing(4);
    m_loadButton = new QPushButton(QStringLiteral("Load"), this);
    m_playButton = new QPushButton(QStringLiteral("▶"), this);
    m_stopButton = new QPushButton(QStringLiteral("■"), this);
    m_loopButton = new QPushButton(this);
    m_loopButton->setCheckable(true);
    m_loopButton->setChecked(true);
    m_loopButton->setIcon(makeLoopIcon());
    m_loopButton->setIconSize(QSize(15, 15));
    m_loopButton->setToolTip(QStringLiteral("Boucle"));
    m_playButton->setFixedWidth(34);
    m_stopButton->setFixedWidth(34);
    m_loopButton->setFixedWidth(34);
    m_loopButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  padding: 4px;"
        "  background: #1a222d;"
        "  border: 1px solid #303848;"
        "  border-radius: 2px;"
        "}"
        "QPushButton:hover {"
        "  background: #243040;"
        "}"
        "QPushButton:checked {"
        "  background: #2f6fbf;"
        "  border: 1px solid #7eb6ff;"
        "}"
        "QPushButton:checked:hover {"
        "  background: #3a7fd4;"
        "}"
        "QPushButton:!checked {"
        "  background: #141a22;"
        "  border: 1px solid #2a3340;"
        "}"));
    controls->addWidget(m_loadButton);
    controls->addWidget(m_playButton);
    controls->addWidget(m_stopButton);
    controls->addWidget(m_loopButton);
    root->addLayout(controls);

    connect(m_loadButton, &QPushButton::clicked, this, &PlayerModuleWidget::onOpenFile);
    connect(m_playButton, &QPushButton::clicked, this, &PlayerModuleWidget::onPlayPause);
    connect(m_stopButton, &QPushButton::clicked, this, &PlayerModuleWidget::onStop);
    connect(m_loopButton, &QPushButton::toggled, this, &PlayerModuleWidget::onLoopToggled);
    connect(m_eyeButton, &QPushButton::toggled, this, &PlayerModuleWidget::onOutputVisibleToggled);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &PlayerModuleWidget::onNameEdited);
    connect(m_seekSlider, &QSlider::sliderPressed, this, [this]() { m_seekDragging = true; });
    connect(m_seekSlider, &QSlider::sliderMoved, this, [this](int value) {
        if (m_seekDragging)
            onSeek(value);
    });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this]() {
        m_seekDragging = false;
        onSeek(m_seekSlider->value());
    });

    setSelected(false);
}

void PlayerModuleWidget::setModule(const std::shared_ptr<PlayerModule> &module)
{
    m_module = module;
    if (!m_module)
        return;
    connect(m_module.get(), &PlayerModule::stateChanged, this,
            &PlayerModuleWidget::onModuleStateChanged);
    connect(m_module.get(), &PlayerModule::frameUpdated, this, &PlayerModuleWidget::refreshFrame);
    connect(m_module.get(), &PlayerModule::positionChanged, this,
            &PlayerModuleWidget::onPositionChanged);
    onModuleStateChanged();
    syncOutputVisible();
    syncNameFromModule();
}

void PlayerModuleWidget::setIndex(int index)
{
    m_index = index;
    if (m_module)
        syncNameFromModule();
    else if (m_nameEdit) {
        const QSignalBlocker b(m_nameEdit);
        m_nameEdit->setText(PlayerModule::defaultNameForIndex(index));
    }
}

void PlayerModuleWidget::syncNameFromModule()
{
    if (!m_nameEdit)
        return;
    const QSignalBlocker b(m_nameEdit);
    if (m_module)
        m_nameEdit->setText(m_module->name());
    else
        m_nameEdit->setText(PlayerModule::defaultNameForIndex(m_index));
}

void PlayerModuleWidget::onNameEdited()
{
    if (!m_nameEdit)
        return;
    const QString text = m_nameEdit->text().trimmed();
    if (m_module) {
        m_module->setName(text);
        syncNameFromModule();
    } else if (text.isEmpty()) {
        const QSignalBlocker b(m_nameEdit);
        m_nameEdit->setText(PlayerModule::defaultNameForIndex(m_index));
    }
    emit projectEdited();
}

void PlayerModuleWidget::setSelected(bool selected)
{
    m_selected = selected;
    applyChrome();
}

void PlayerModuleWidget::applyChrome()
{
    if (m_dropHighlight) {
        setStyleSheet(QStringLiteral(
            "#playerModule {"
            "  background: #151c26;"
            "  border: 2px solid #6ec1ff;"
            "  border-radius: 2px;"
            "}"));
        return;
    }
    setStyleSheet(m_selected ? QStringLiteral(
                                   "#playerModule {"
                                   "  background: #151c26;"
                                   "  border: 2px solid #4f9cff;"
                                   "  border-radius: 2px;"
                                   "}")
                             : QStringLiteral(
                                   "#playerModule {"
                                   "  background: #10151c;"
                                   "  border: 1px solid #232b36;"
                                   "  border-radius: 2px;"
                                   "}"));
}

void PlayerModuleWidget::setListMode(bool listMode)
{
    const bool changed = (m_listMode != listMode);
    m_listMode = listMode;
    if (changed && m_preview) {
        m_preview->setVisible(!listMode);
        if (listMode)
            m_preview->clearFrame();
    }
    setMinimumWidth(0);
    if (listMode) {
        setMinimumHeight(96);
        setMaximumHeight(118);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        setMinimumHeight(120);
        setMaximumHeight(QWIDGETSIZE_MAX);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    updateGeometry();
}

void PlayerModuleWidget::refreshFrame()
{
    if (m_listMode || !m_module || !m_preview || !m_preview->isVisible())
        return;
    m_preview->setFrame(m_module->currentFrame());
}

void PlayerModuleWidget::mousePressEvent(QMouseEvent *event)
{
    emit selected(m_index);
    QFrame::mousePressEvent(event);
}

void PlayerModuleWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateFileLabelElide();
}

void PlayerModuleWidget::setFileLabelText(const QString &text)
{
    m_fileFullText = text;
    if (m_fileLabel) {
        m_fileLabel->setToolTip(text == QStringLiteral("—") ? QString() : text);
        updateFileLabelElide();
    }
}

void PlayerModuleWidget::updateFileLabelElide()
{
    if (!m_fileLabel)
        return;
    const int w = m_fileLabel->width();
    if (w < 8) {
        m_fileLabel->setText(m_fileFullText);
        return;
    }
    const QFontMetrics fm(m_fileLabel->font());
    m_fileLabel->setText(fm.elidedText(m_fileFullText, Qt::ElideMiddle, w));
}

QString PlayerModuleWidget::mediaPathFromMime(const QMimeData *mime)
{
    if (!mime || !mime->hasUrls())
        return {};
    for (const QUrl &url : mime->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (MediaBank::isSupportedMediaFile(path))
            return path;
    }
    return {};
}

void PlayerModuleWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!mediaPathFromMime(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
        m_dropHighlight = true;
        applyChrome();
        emit selected(m_index);
    } else {
        event->ignore();
    }
}

void PlayerModuleWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (!mediaPathFromMime(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void PlayerModuleWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dropHighlight = false;
    applyChrome();
    QFrame::dragLeaveEvent(event);
}

void PlayerModuleWidget::dropEvent(QDropEvent *event)
{
    m_dropHighlight = false;
    applyChrome();
    const QString path = mediaPathFromMime(event->mimeData());
    if (path.isEmpty()) {
        event->ignore();
        return;
    }
    emit selected(m_index);
    emit mediaDropped(m_index, path);
    event->acceptProposedAction();
}

void PlayerModuleWidget::setPauseBlink(bool active, bool phaseOn)
{
    m_pauseBlinkActive = active;
    m_pauseBlinkPhase = phaseOn;
    applyPlayButtonStyle();
}

void PlayerModuleWidget::applyPlayButtonStyle()
{
    if (!m_playButton)
        return;
    if (m_pauseBlinkActive) {
        m_playButton->setStyleSheet(
            m_pauseBlinkPhase
                ? QStringLiteral(
                      "QPushButton {"
                      "  background: #e07020;"
                      "  border: 1px solid #ffb060;"
                      "  color: #1a1008;"
                      "  font-weight: 800;"
                      "}")
                : QStringLiteral(
                      "QPushButton {"
                      "  background: #4a2a10;"
                      "  border: 1px solid #a06030;"
                      "  color: #ffc080;"
                      "  font-weight: 800;"
                      "}"));
    } else {
        m_playButton->setStyleSheet(QString());
    }
}

void PlayerModuleWidget::onPlayPause()
{
    if (!m_module)
        return;
    const bool wasPlaying = m_module->decoder()->isPlaying();
    m_module->togglePlayPause();
    if (wasPlaying)
        emit pausePressed();
    else
        emit playPressed();
}

void PlayerModuleWidget::onStop()
{
    if (m_module)
        m_module->stop();
    emit stopPressed();
}

void PlayerModuleWidget::onLoopToggled(bool checked)
{
    if (m_module)
        m_module->decoder()->setLoop(checked);
    emit projectEdited();
}

void PlayerModuleWidget::onSeek(int value)
{
    if (!m_module || !m_module->decoder()->isOpen())
        return;
    const qint64 dur = m_module->decoder()->durationMs();
    if (dur <= 0)
        return;
    m_module->decoder()->seekMs(static_cast<qint64>((value / 1000.0) * dur));
}

void PlayerModuleWidget::onOpenFile()
{
    emit openFileRequested(m_index);
}

void PlayerModuleWidget::onOutputVisibleToggled(bool visible)
{
    if (m_module)
        m_module->transform().visible = visible;
    updateEyeIcon();
    emit projectEdited();
}

void PlayerModuleWidget::syncOutputVisible()
{
    if (!m_eyeButton)
        return;
    const bool visible = !m_module || m_module->transform().visible;
    const QSignalBlocker b(m_eyeButton);
    m_eyeButton->setChecked(visible);
    updateEyeIcon();
}

void PlayerModuleWidget::updateEyeIcon()
{
    if (!m_eyeButton)
        return;
    const bool visible = m_eyeButton->isChecked();
    m_eyeButton->setIcon(makeEyeIcon(visible));
    m_eyeButton->setToolTip(visible ? QStringLiteral("Masquer dans la sortie (Program / NDI / Spout)")
                                    : QStringLiteral("Afficher dans la sortie (Program / NDI / Spout)"));
}

void PlayerModuleWidget::onModuleStateChanged()
{
    if (!m_module)
        return;
    const auto *dec = m_module->decoder();
    if (dec->isOpen()) {
        setFileLabelText(QFileInfo(dec->path()).fileName());
        m_seekSlider->setEnabled(true);
        m_playButton->setText(dec->isPlaying() ? QStringLiteral("❚❚") : QStringLiteral("▶"));
        applyPlayButtonStyle();
    } else {
        if (m_module && !m_module->lastError().isEmpty())
            setFileLabelText(m_module->lastError());
        else
            setFileLabelText(QStringLiteral("—"));
        m_seekSlider->setEnabled(false);
        m_playButton->setText(QStringLiteral("▶"));
        m_preview->clearFrame();
        applyPlayButtonStyle();
    }
    const QSignalBlocker blocker(m_loopButton);
    m_loopButton->setChecked(dec->isLooping());
}

void PlayerModuleWidget::onPositionChanged(qint64 ms, qint64 durationMs)
{
    if (m_seekDragging || durationMs <= 0)
        return;
    m_seekSlider->setValue(static_cast<int>((ms * 1000.0) / durationMs));
}
