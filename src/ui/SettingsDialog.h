#pragma once

#include "output/NdiSender.h"

#include <QDialog>
#include <QSize>

class OscServer;
class MidiController;
class OutputManager;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(OscServer *osc, MidiController *midi, OutputManager *output, QSize canvasSize,
                   int fps, QWidget *parent = nullptr);

    QSize selectedCanvasSize() const;
    int selectedFps() const;

signals:
    void applyComposition(QSize size, int fps);
    void autosaveChanged(bool enabled);
    void outputsChanged();

private slots:
    void onPresetChanged(int index);
    void onApplyComposition();
    void onOscToggled(bool enabled);
    void onMidiToggled(bool enabled);
    void onRefreshMidi();
    void onStartLearn();
    void onClearBindings();
    void reloadBindingsList();
    void syncOscUi();
    void syncMidiUi();
    void onOutputsEdited();

private:
    void buildCompositionTab(QWidget *tab);
    void buildOutputsTab(QWidget *tab);
    void buildOscTab(QWidget *tab);
    void buildMidiTab(QWidget *tab);
    void fillPresets();
    void syncOutputsUi();
    void startOsc();
    void stopOsc();
    void startMidi();
    void stopMidi();

    OscServer *m_osc = nullptr;
    MidiController *m_midi = nullptr;
    OutputManager *m_output = nullptr;

    QComboBox *m_presetCombo = nullptr;
    QSpinBox *m_customW = nullptr;
    QSpinBox *m_customH = nullptr;
    QSpinBox *m_fpsSpin = nullptr;
    QLabel *m_compHint = nullptr;
    QCheckBox *m_autosaveCheck = nullptr;

    QPushButton *m_ndiEnable = nullptr;
    QLineEdit *m_ndiNameEdit = nullptr;
    QPushButton *m_ndiAlpha = nullptr;
    QComboBox *m_ndiBandwidth = nullptr;
    QPushButton *m_spoutEnable = nullptr;
    QLineEdit *m_spoutNameEdit = nullptr;

    QPushButton *m_oscEnable = nullptr;
    QSpinBox *m_oscPort = nullptr;
    QLabel *m_oscStatus = nullptr;

    QPushButton *m_midiEnable = nullptr;
    QComboBox *m_midiPorts = nullptr;
    QLabel *m_midiStatus = nullptr;
    QComboBox *m_learnAction = nullptr;
    QListWidget *m_bindingsList = nullptr;
    QPlainTextEdit *m_log = nullptr;
};
