#pragma once

#include "compose/ClipTransform.h"
#include "compose/OutputCanvas.h"

#include <QWidget>
#include <memory>

class PlayerModule;
class QDoubleSpinBox;
class QSlider;
class QCheckBox;
class QLabel;
class QComboBox;

class ClipTransformPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ClipTransformPanel(QWidget *parent = nullptr);

    void setModule(const std::shared_ptr<PlayerModule> &module);
    void reloadFromModule();
    void setAvailableSlices(const QStringList &sliceNames);

signals:
    void transformChanged();
    // Empty sliceName = unlink (no align). Non-empty = link + apply align.
    void sliceLinkChanged(const QString &sliceName, SliceAlignMode mode);

private slots:
    void onUiChanged();
    void onResetCrop();
    void onResetTransform();
    void onSliceLinkUiChanged();

private:
    void blockUi(bool block);
    ClipTransform readUi() const;
    void writeUi(const ClipTransform &t);
    void updateModeVisibility();
    SliceAlignMode currentAlignMode() const;
    void setAlignModeUi(SliceAlignMode mode);

    std::shared_ptr<PlayerModule> m_module;
    QLabel *m_moduleLabel = nullptr;
    QDoubleSpinBox *m_cropL = nullptr;
    QDoubleSpinBox *m_cropT = nullptr;
    QDoubleSpinBox *m_cropR = nullptr;
    QDoubleSpinBox *m_cropB = nullptr;
    QDoubleSpinBox *m_posX = nullptr;
    QDoubleSpinBox *m_posY = nullptr;
    QDoubleSpinBox *m_scale = nullptr;
    QDoubleSpinBox *m_scaleY = nullptr;
    QSlider *m_opacity = nullptr;
    QCheckBox *m_visible = nullptr;
    QComboBox *m_sliceCombo = nullptr;
    QComboBox *m_alignModeCombo = nullptr;
    QLabel *m_alignModeLabel = nullptr;
    QCheckBox *m_maskToSlice = nullptr;
    bool m_updating = false;
};
