#include "ui/ClipTransformPanel.h"
#include "media/PlayerModule.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QDoubleSpinBox *makeSpin(QWidget *parent, double min, double max, double step, int decimals)
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    return s;
}

} // namespace

ClipTransformPanel::ClipTransformPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *box = new QGroupBox(QStringLiteral("Clip (module sélectionné)"), this);
    auto *form = new QFormLayout(box);

    m_moduleLabel = new QLabel(QStringLiteral("—"), box);
    m_moduleLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #e8ecf2;"));
    form->addRow(QStringLiteral("Module"), m_moduleLabel);

    m_cropL = makeSpin(box, 0.0, 0.49, 0.01, 3);
    m_cropT = makeSpin(box, 0.0, 0.49, 0.01, 3);
    m_cropR = makeSpin(box, 0.0, 0.49, 0.01, 3);
    m_cropB = makeSpin(box, 0.0, 0.49, 0.01, 3);
    form->addRow(QStringLiteral("Crop L"), m_cropL);
    form->addRow(QStringLiteral("Crop T"), m_cropT);
    form->addRow(QStringLiteral("Crop R"), m_cropR);
    form->addRow(QStringLiteral("Crop B"), m_cropB);

    m_posX = makeSpin(box, -10000.0, 10000.0, 1.0, 1);
    m_posY = makeSpin(box, -10000.0, 10000.0, 1.0, 1);
    m_scale = makeSpin(box, 0.01, 20.0, 0.05, 3);
    m_scaleY = makeSpin(box, 0.01, 20.0, 0.05, 3);
    m_scale->setToolTip(QStringLiteral("Échelle largeur (1 = contain canvas)"));
    m_scaleY->setToolTip(QStringLiteral("Échelle hauteur (égale à Scale X sauf Stretch)"));
    form->addRow(QStringLiteral("Pos X"), m_posX);
    form->addRow(QStringLiteral("Pos Y"), m_posY);
    form->addRow(QStringLiteral("Scale X"), m_scale);
    form->addRow(QStringLiteral("Scale Y"), m_scaleY);

    m_opacity = new QSlider(Qt::Horizontal, box);
    m_opacity->setRange(0, 100);
    m_opacity->setValue(100);
    form->addRow(QStringLiteral("Opacity"), m_opacity);

    m_visible = new QCheckBox(QStringLiteral("Visible"), box);
    m_visible->setChecked(true);
    form->addRow(QString(), m_visible);

    auto *btns = new QHBoxLayout();
    auto *resetCrop = new QPushButton(QStringLiteral("Reset crop"), box);
    auto *resetXf = new QPushButton(QStringLiteral("Reset pos"), box);
    btns->addWidget(resetCrop);
    btns->addWidget(resetXf);
    form->addRow(btns);

    m_sliceCombo = new QComboBox(box);
    m_sliceCombo->setToolTip(
        QStringLiteral("Lier ce module au inputRect d'une slice (Output Map)"));
    m_sliceCombo->addItem(QStringLiteral("—"), QString());
    form->addRow(QStringLiteral("Slice"), m_sliceCombo);

    m_alignModeLabel = new QLabel(QStringLiteral("Mode"), box);
    m_alignModeCombo = new QComboBox(box);
    m_alignModeCombo->addItem(QStringLiteral("Fit — contenir"), static_cast<int>(SliceAlignMode::Fit));
    m_alignModeCombo->addItem(QStringLiteral("Fill — couvrir"), static_cast<int>(SliceAlignMode::Fill));
    m_alignModeCombo->addItem(QStringLiteral("Stretch — déformer"),
                              static_cast<int>(SliceAlignMode::Stretch));
    m_alignModeCombo->setItemData(0, QStringLiteral("Le média entier reste visible dans la slice"),
                                  Qt::ToolTipRole);
    m_alignModeCombo->setItemData(
        1, QStringLiteral("La slice est entièrement couverte ; bords éventuellement hors zone"),
        Qt::ToolTipRole);
    m_alignModeCombo->setItemData(
        2, QStringLiteral("Étire largeur et hauteur indépendamment sur la slice"), Qt::ToolTipRole);
    form->addRow(m_alignModeLabel, m_alignModeCombo);

    m_maskToSlice = new QCheckBox(QStringLiteral("Mask hors slice"), box);
    m_maskToSlice->setChecked(true);
    m_maskToSlice->setToolTip(
        QStringLiteral("Découpe la vidéo au inputRect de la slice (utile en Fill)"));
    form->addRow(QString(), m_maskToSlice);

    root->addWidget(box);

    const auto connectSpin = [this](QDoubleSpinBox *s) {
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &ClipTransformPanel::onUiChanged);
    };
    connectSpin(m_cropL);
    connectSpin(m_cropT);
    connectSpin(m_cropR);
    connectSpin(m_cropB);
    connectSpin(m_posX);
    connectSpin(m_posY);
    connectSpin(m_scale);
    connectSpin(m_scaleY);
    connect(m_opacity, &QSlider::valueChanged, this, &ClipTransformPanel::onUiChanged);
    connect(m_visible, &QCheckBox::toggled, this, &ClipTransformPanel::onUiChanged);
    connect(resetCrop, &QPushButton::clicked, this, &ClipTransformPanel::onResetCrop);
    connect(resetXf, &QPushButton::clicked, this, &ClipTransformPanel::onResetTransform);
    connect(m_sliceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ClipTransformPanel::onSliceLinkUiChanged);
    connect(m_alignModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ClipTransformPanel::onSliceLinkUiChanged);
    connect(m_maskToSlice, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updating || !m_module)
            return;
        m_module->setMaskToSlice(on);
        emit transformChanged();
    });

    updateModeVisibility();
}

void ClipTransformPanel::setModule(const std::shared_ptr<PlayerModule> &module)
{
    m_module = module;
    reloadFromModule();
}

void ClipTransformPanel::setAvailableSlices(const QStringList &sliceNames)
{
    const QString current = m_sliceCombo->currentData().toString();
    QSignalBlocker b(m_sliceCombo);
    m_sliceCombo->clear();
    m_sliceCombo->addItem(QStringLiteral("—"), QString());
    for (const QString &name : sliceNames) {
        if (!name.isEmpty())
            m_sliceCombo->addItem(name, name);
    }
    const int idx = m_sliceCombo->findData(current);
    m_sliceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    updateModeVisibility();
}

void ClipTransformPanel::reloadFromModule()
{
    if (!m_module) {
        m_moduleLabel->setText(QStringLiteral("—"));
        return;
    }
    m_moduleLabel->setText(m_module->name());
    writeUi(m_module->transform());

    QSignalBlocker b1(m_sliceCombo);
    QSignalBlocker b2(m_alignModeCombo);
    QSignalBlocker b3(m_maskToSlice);
    const QString link = m_module->linkedSliceName();
    int idx = m_sliceCombo->findData(link);
    if (idx < 0 && !link.isEmpty()) {
        m_sliceCombo->addItem(link, link);
        idx = m_sliceCombo->findData(link);
    }
    m_sliceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    setAlignModeUi(m_module->sliceAlignMode());
    m_maskToSlice->setChecked(m_module->maskToSlice());
    updateModeVisibility();
}

void ClipTransformPanel::updateModeVisibility()
{
    const bool hasSlice = m_sliceCombo && !m_sliceCombo->currentData().toString().isEmpty();
    if (m_alignModeLabel)
        m_alignModeLabel->setVisible(hasSlice);
    if (m_alignModeCombo)
        m_alignModeCombo->setVisible(hasSlice);
    if (m_maskToSlice)
        m_maskToSlice->setVisible(hasSlice);
}

SliceAlignMode ClipTransformPanel::currentAlignMode() const
{
    if (!m_alignModeCombo)
        return SliceAlignMode::Fit;
    return static_cast<SliceAlignMode>(m_alignModeCombo->currentData().toInt());
}

void ClipTransformPanel::setAlignModeUi(SliceAlignMode mode)
{
    if (!m_alignModeCombo)
        return;
    const int idx = m_alignModeCombo->findData(static_cast<int>(mode));
    m_alignModeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void ClipTransformPanel::blockUi(bool block)
{
    m_updating = block;
}

ClipTransform ClipTransformPanel::readUi() const
{
    ClipTransform t;
    t.cropLeft = static_cast<float>(m_cropL->value());
    t.cropTop = static_cast<float>(m_cropT->value());
    t.cropRight = static_cast<float>(m_cropR->value());
    t.cropBottom = static_cast<float>(m_cropB->value());
    t.posX = static_cast<float>(m_posX->value());
    t.posY = static_cast<float>(m_posY->value());
    t.scale = static_cast<float>(m_scale->value());
    t.scaleY = static_cast<float>(m_scaleY->value());
    t.opacity = m_opacity->value() / 100.f;
    t.visible = m_visible->isChecked();
    return t;
}

void ClipTransformPanel::writeUi(const ClipTransform &t)
{
    blockUi(true);
    m_cropL->setValue(t.cropLeft);
    m_cropT->setValue(t.cropTop);
    m_cropR->setValue(t.cropRight);
    m_cropB->setValue(t.cropBottom);
    m_posX->setValue(t.posX);
    m_posY->setValue(t.posY);
    m_scale->setValue(t.scale);
    m_scaleY->setValue(t.scaleY);
    m_opacity->setValue(static_cast<int>(t.opacity * 100.f));
    m_visible->setChecked(t.visible);
    blockUi(false);
}

void ClipTransformPanel::onUiChanged()
{
    if (m_updating || !m_module)
        return;
    m_module->transform() = readUi();
    emit transformChanged();
}

void ClipTransformPanel::onResetCrop()
{
    if (!m_module)
        return;
    auto &t = m_module->transform();
    t.cropLeft = t.cropTop = t.cropRight = t.cropBottom = 0.f;
    writeUi(t);
    emit transformChanged();
}

void ClipTransformPanel::onResetTransform()
{
    if (!m_module)
        return;
    auto &t = m_module->transform();
    t.posX = 960.f;
    t.posY = 540.f;
    t.scale = 1.f;
    t.scaleY = 1.f;
    t.opacity = 1.f;
    writeUi(t);
    emit transformChanged();
}

void ClipTransformPanel::onSliceLinkUiChanged()
{
    if (m_updating)
        return;
    updateModeVisibility();
    const QString name = m_sliceCombo->currentData().toString();
    const SliceAlignMode mode = currentAlignMode();
    if (m_module) {
        m_module->setLinkedSliceName(name);
        m_module->setSliceAlignMode(mode);
    }
    emit sliceLinkChanged(name, mode);
}
