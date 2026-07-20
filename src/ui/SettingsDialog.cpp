#include "ui/SettingsDialog.h"
#include "control/MidiController.h"
#include "control/OscServer.h"
#include "output/OutputManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaType>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QStringList learnActions()
{
    QStringList a;
    a << QStringLiteral("output_toggle") << QStringLiteral("output");
    a << QStringLiteral("play_all") << QStringLiteral("pause_all") << QStringLiteral("stop_all")
      << QStringLiteral("toggle_all");
    for (int i = 0; i < MP_MODULE_MAX; ++i) {
        a << QStringLiteral("module.%1.toggle").arg(i);
        a << QStringLiteral("module.%1.play").arg(i);
        a << QStringLiteral("module.%1.stop").arg(i);
        a << QStringLiteral("module.%1.opacity").arg(i);
        a << QStringLiteral("module.%1.scale").arg(i);
    }
    return a;
}

QString bindingText(const MidiBinding &b)
{
    const QString type =
        b.type == MidiBinding::Type::NoteOn ? QStringLiteral("Note") : QStringLiteral("CC");
    return QStringLiteral("%1 ch%2 #%3 → %4")
        .arg(type)
        .arg(b.channel)
        .arg(b.number)
        .arg(b.action);
}

constexpr int kCustomPresetRole = -1;

bool isCustomPreset(const QComboBox *combo)
{
    const QVariant data = combo->currentData();
    if (!data.isValid())
        return true;
    if (data.userType() == QMetaType::Int && data.toInt() == kCustomPresetRole)
        return true;
    if (data.canConvert<QSize>()) {
        const QSize s = data.toSize();
        return !s.isValid() || s.isEmpty();
    }
    return true;
}

QPushButton *makeToggleButton(const QString &text, QWidget *parent)
{
    static const QString kStyle = QStringLiteral(
        "QPushButton {"
        "  padding: 6px 14px; font-weight: 700; font-size: 11px;"
        "  background: #141a22; color: #8b95a8; border: 1px solid #2a3340;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:checked {"
        "  background: #1e3a5f; color: #e8eef8; border-color: #4f9cff;"
        "}"
        "QPushButton:hover { background: #1a2430; color: #c5cedd; }");
    auto *btn = new QPushButton(text, parent);
    btn->setCheckable(true);
    btn->setStyleSheet(kStyle);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

} // namespace

SettingsDialog::SettingsDialog(OscServer *osc, MidiController *midi, OutputManager *output,
                               QSize canvasSize, int fps, QWidget *parent)
    : QDialog(parent)
    , m_osc(osc)
    , m_midi(midi)
    , m_output(output)
{
    setWindowTitle(QStringLiteral("Paramètres"));
    resize(640, 600);

    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    auto *compTab = new QWidget(tabs);
    auto *outTab = new QWidget(tabs);
    auto *oscTab = new QWidget(tabs);
    auto *midiTab = new QWidget(tabs);
    buildCompositionTab(compTab);
    buildOutputsTab(outTab);
    buildOscTab(oscTab);
    buildMidiTab(midiTab);
    tabs->addTab(compTab, QStringLiteral("Composition"));
    tabs->addTab(outTab, QStringLiteral("Sorties"));
    tabs->addTab(oscTab, QStringLiteral("OSC"));
    tabs->addTab(midiTab, QStringLiteral("MIDI"));
    root->addWidget(tabs, 1);

    auto *bottom = new QHBoxLayout();
    bottom->addStretch();
    auto *closeBtn = new QPushButton(QStringLiteral("Fermer"), this);
    bottom->addWidget(closeBtn);
    root->addLayout(bottom);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    // Init composition values
    bool matched = false;
    for (int i = 0; i < m_presetCombo->count(); ++i) {
        if (m_presetCombo->itemData(i).toSize() == canvasSize) {
            m_presetCombo->setCurrentIndex(i);
            matched = true;
            break;
        }
    }
    if (!matched) {
        for (int i = 0; i < m_presetCombo->count(); ++i) {
            if (!m_presetCombo->itemData(i).isValid()
                || !m_presetCombo->itemData(i).canConvert<QSize>()
                || m_presetCombo->itemData(i).toSize().isEmpty()) {
                m_presetCombo->setCurrentIndex(i);
                break;
            }
        }
        m_customW->setValue(canvasSize.width());
        m_customH->setValue(canvasSize.height());
    }
    m_fpsSpin->setValue(fps);
    onPresetChanged(m_presetCombo->currentIndex());

    // OSC / MIDI enable from settings (sans redémarrer à l’ouverture)
    QSettings s;
    {
        const QSignalBlocker b(m_oscEnable);
        m_oscEnable->setChecked(s.value(QStringLiteral("osc/enabled"), true).toBool());
    }
    m_oscPort->setValue(s.value(QStringLiteral("osc/port"), 7000).toInt());
    {
        const QSignalBlocker b(m_midiEnable);
        m_midiEnable->setChecked(s.value(QStringLiteral("midi/enabled"), true).toBool());
    }

    if (m_osc) {
        connect(m_osc, &OscServer::statusChanged, this, [this](const QString &st) {
            m_oscStatus->setText(st);
            syncOscUi();
        });
        connect(m_osc, &OscServer::messageReceived, this,
                [this](const QString &addr, const QVariantList &args) {
                    if (m_log)
                        m_log->appendPlainText(
                            QStringLiteral("OSC ← %1 (%2)").arg(addr).arg(args.size()));
                });
    }
    if (m_midi) {
        connect(m_midi, &MidiController::statusChanged, this, [this](const QString &st) {
            m_midiStatus->setText(st);
            syncMidiUi();
        });
        connect(m_midi, &MidiController::learned, this, [this](const MidiBinding &b) {
            if (m_log)
                m_log->appendPlainText(QStringLiteral("MIDI learned %1").arg(bindingText(b)));
            reloadBindingsList();
        });
        connect(m_midi, &MidiController::actionTriggered, this,
                [this](const QString &a, float v) {
                    if (m_log)
                        m_log->appendPlainText(QStringLiteral("MIDI %1 = %2").arg(a).arg(v));
                });
    }

    syncOscUi();
    onRefreshMidi();
    syncMidiUi();
    reloadBindingsList();
    syncOutputsUi();
}

void SettingsDialog::fillPresets()
{
    m_presetCombo->clear();
    m_presetCombo->addItem(QStringLiteral("1280 × 720"), QSize(1280, 720));
    m_presetCombo->addItem(QStringLiteral("1920 × 1080"), QSize(1920, 1080));
    m_presetCombo->addItem(QStringLiteral("1920 × 1200"), QSize(1920, 1200));
    m_presetCombo->addItem(QStringLiteral("2560 × 1440"), QSize(2560, 1440));
    m_presetCombo->addItem(QStringLiteral("3840 × 1080"), QSize(3840, 1080));
    m_presetCombo->addItem(QStringLiteral("3840 × 2160"), QSize(3840, 2160));
    m_presetCombo->addItem(QStringLiteral("4096 × 2160"), QSize(4096, 2160));
    m_presetCombo->addItem(QStringLiteral("Personnalisée…")); // no size data = custom
}

void SettingsDialog::buildCompositionTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    auto *box = new QGroupBox(QStringLiteral("Résolution de composition (matrice)"), tab);
    auto *form = new QFormLayout(box);

    m_presetCombo = new QComboBox(box);
    fillPresets();
    form->addRow(QStringLiteral("Préréglage"), m_presetCombo);

    auto *customRow = new QHBoxLayout();
    m_customW = new QSpinBox(box);
    m_customH = new QSpinBox(box);
    m_customW->setRange(16, 16384);
    m_customH->setRange(16, 16384);
    m_customW->setValue(1920);
    m_customH->setValue(1080);
    customRow->addWidget(m_customW);
    customRow->addWidget(new QLabel(QStringLiteral("×"), box));
    customRow->addWidget(m_customH);
    form->addRow(QStringLiteral("Personnalisée"), customRow);

    m_fpsSpin = new QSpinBox(box);
    m_fpsSpin->setRange(1, 120);
    m_fpsSpin->setSuffix(QStringLiteral(" fps"));
    form->addRow(QStringLiteral("FPS cible"), m_fpsSpin);

    auto *applyBtn = new QPushButton(QStringLiteral("Appliquer à la composition"), box);
    form->addRow(applyBtn);
    m_compHint = new QLabel(
        QStringLiteral("La résolution définit la matrice Program / NDI / Spout."), box);
    m_compHint->setWordWrap(true);
    m_compHint->setStyleSheet(QStringLiteral("color: #9aa3b2;"));
    form->addRow(m_compHint);

    m_autosaveCheck = new QCheckBox(QStringLiteral("Autosave projet (toutes les 60 s si modifié)"),
                                    box);
    m_autosaveCheck->setChecked(
        QSettings().value(QStringLiteral("project/autosave"), false).toBool());
    m_autosaveCheck->setToolTip(
        QStringLiteral("Enregistre automatiquement le projet courant lorsqu'il y a des "
                       "modifications non sauvées."));
    form->addRow(m_autosaveCheck);

    layout->addWidget(box);
    layout->addStretch();

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SettingsDialog::onPresetChanged);
    connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyComposition);
    connect(m_autosaveCheck, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("project/autosave"), on);
        emit autosaveChanged(on);
    });
}

void SettingsDialog::buildOutputsTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    auto *box = new QGroupBox(QStringLiteral("Sorties Program (NDI / Spout)"), tab);
    auto *form = new QFormLayout(box);

    m_ndiEnable = makeToggleButton(QStringLiteral("Activer NDI"), box);
    m_ndiNameEdit = new QLineEdit(box);
    m_ndiNameEdit->setPlaceholderText(QStringLiteral("Nom source NDI"));
    m_ndiAlpha = makeToggleButton(QStringLiteral("Canal alpha (BGRA)"), box);
    m_ndiAlpha->setToolTip(
        QStringLiteral("Envoie la composition en BGRA. Requis pour le key dans Resolume."));
    m_ndiBandwidth = new QComboBox(box);
    m_ndiBandwidth->addItem(QStringLiteral("Débit max"), static_cast<int>(NdiBandwidth::Highest));
    m_ndiBandwidth->addItem(QStringLiteral("Débit bas"), static_cast<int>(NdiBandwidth::Lowest));

    m_spoutEnable = makeToggleButton(QStringLiteral("Activer Spout"), box);
    m_spoutNameEdit = new QLineEdit(box);
    m_spoutNameEdit->setPlaceholderText(QStringLiteral("Nom source Spout"));

    form->addRow(m_ndiEnable);
    form->addRow(QStringLiteral("Nom NDI"), m_ndiNameEdit);
    form->addRow(m_ndiAlpha);
    form->addRow(QStringLiteral("Débit NDI"), m_ndiBandwidth);
    form->addRow(m_spoutEnable);
    form->addRow(QStringLiteral("Nom Spout"), m_spoutNameEdit);

    auto *hint = new QLabel(
        QStringLiteral("Ces sorties s’appliquent lorsque OUTPUT est ON AIR."), box);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #9aa3b2;"));
    form->addRow(hint);

    layout->addWidget(box);
    layout->addStretch();

    connect(m_ndiEnable, &QPushButton::toggled, this, &SettingsDialog::onOutputsEdited);
    connect(m_spoutEnable, &QPushButton::toggled, this, &SettingsDialog::onOutputsEdited);
    connect(m_ndiAlpha, &QPushButton::toggled, this, &SettingsDialog::onOutputsEdited);
    connect(m_ndiBandwidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SettingsDialog::onOutputsEdited);
    connect(m_ndiNameEdit, &QLineEdit::editingFinished, this, &SettingsDialog::onOutputsEdited);
    connect(m_spoutNameEdit, &QLineEdit::editingFinished, this, &SettingsDialog::onOutputsEdited);
}

void SettingsDialog::syncOutputsUi()
{
    if (!m_output)
        return;
    const QSignalBlocker b1(m_ndiEnable);
    const QSignalBlocker b2(m_spoutEnable);
    const QSignalBlocker b3(m_ndiAlpha);
    const QSignalBlocker b4(m_ndiBandwidth);
    const QSignalBlocker b5(m_ndiNameEdit);
    const QSignalBlocker b6(m_spoutNameEdit);
    m_ndiEnable->setChecked(m_output->ndiEnabled());
    m_spoutEnable->setChecked(m_output->spoutEnabled());
    m_ndiAlpha->setChecked(m_output->ndiSendAlpha());
    m_ndiBandwidth->setCurrentIndex(m_output->ndiBandwidth() == NdiBandwidth::Lowest ? 1 : 0);
    m_ndiNameEdit->setText(m_output->ndiName());
    m_spoutNameEdit->setText(m_output->spoutName());
}

void SettingsDialog::onOutputsEdited()
{
    if (!m_output)
        return;
    m_output->setNdiEnabled(m_ndiEnable->isChecked());
    m_output->setSpoutEnabled(m_spoutEnable->isChecked());
    m_output->setNdiName(m_ndiNameEdit->text().trimmed().isEmpty()
                             ? QStringLiteral("MultiPlayer")
                             : m_ndiNameEdit->text().trimmed());
    m_output->setSpoutName(m_spoutNameEdit->text().trimmed().isEmpty()
                               ? QStringLiteral("MultiPlayer")
                               : m_spoutNameEdit->text().trimmed());
    m_output->setNdiSendAlpha(m_ndiAlpha->isChecked());
    m_output->setNdiBandwidth(static_cast<NdiBandwidth>(m_ndiBandwidth->currentData().toInt()));
    emit outputsChanged();
}

void SettingsDialog::buildOscTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    auto *box = new QGroupBox(QStringLiteral("OSC (UDP)"), tab);
    auto *form = new QFormLayout(box);

    m_oscEnable = makeToggleButton(QStringLiteral("Activer OSC"), box);
    m_oscEnable->setToolTip(QStringLiteral("Démarre ou arrête le serveur OSC automatiquement"));
    form->addRow(m_oscEnable);

    m_oscPort = new QSpinBox(box);
    m_oscPort->setRange(1024, 65535);
    form->addRow(QStringLiteral("Port"), m_oscPort);

    m_oscStatus = new QLabel(QStringLiteral("OSC: —"), box);
    form->addRow(QStringLiteral("État"), m_oscStatus);

    auto *hint = new QLabel(
        QStringLiteral("Le service démarre et s’arrête avec le bouton Activer."), box);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #9aa3b2;"));
    form->addRow(hint);

    layout->addWidget(box);
    layout->addStretch();

    connect(m_oscEnable, &QPushButton::toggled, this, &SettingsDialog::onOscToggled);
    connect(m_oscPort, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (m_oscEnable && m_oscEnable->isChecked())
            startOsc();
    });
}

void SettingsDialog::buildMidiTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    auto *box = new QGroupBox(QStringLiteral("MIDI"), tab);
    auto *form = new QFormLayout(box);

    m_midiEnable = makeToggleButton(QStringLiteral("Activer MIDI"), box);
    m_midiEnable->setToolTip(QStringLiteral("Ouvre ou ferme le port MIDI automatiquement"));
    form->addRow(m_midiEnable);

    auto *portRow = new QHBoxLayout();
    m_midiPorts = new QComboBox(box);
    auto *refresh = new QPushButton(QStringLiteral("Rafraîchir"), box);
    portRow->addWidget(m_midiPorts, 1);
    portRow->addWidget(refresh);
    form->addRow(QStringLiteral("Périphérique"), portRow);

    m_midiStatus = new QLabel(QStringLiteral("MIDI: —"), box);
    form->addRow(QStringLiteral("État"), m_midiStatus);

    auto *hint = new QLabel(
        QStringLiteral("Le port MIDI s’ouvre et se ferme avec le bouton Activer."), box);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #9aa3b2;"));
    form->addRow(hint);

    auto *learnRow = new QHBoxLayout();
    m_learnAction = new QComboBox(box);
    m_learnAction->addItems(learnActions());
    auto *learnBtn = new QPushButton(QStringLiteral("Learn"), box);
    auto *clearBtn = new QPushButton(QStringLiteral("Clear maps"), box);
    learnRow->addWidget(m_learnAction, 1);
    learnRow->addWidget(learnBtn);
    learnRow->addWidget(clearBtn);
    form->addRow(QStringLiteral("Learn"), learnRow);

    m_bindingsList = new QListWidget(box);
    form->addRow(QStringLiteral("Mappings"), m_bindingsList);

    layout->addWidget(box);

    m_log = new QPlainTextEdit(tab);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(150);
    m_log->setPlaceholderText(QStringLiteral("Log OSC / MIDI…"));
    layout->addWidget(m_log, 1);

    connect(m_midiEnable, &QPushButton::toggled, this, &SettingsDialog::onMidiToggled);
    connect(refresh, &QPushButton::clicked, this, &SettingsDialog::onRefreshMidi);
    connect(m_midiPorts, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_midiEnable && m_midiEnable->isChecked())
            startMidi();
    });
    connect(learnBtn, &QPushButton::clicked, this, &SettingsDialog::onStartLearn);
    connect(clearBtn, &QPushButton::clicked, this, &SettingsDialog::onClearBindings);
}

QSize SettingsDialog::selectedCanvasSize() const
{
    if (isCustomPreset(m_presetCombo))
        return QSize(m_customW->value(), m_customH->value());
    return m_presetCombo->currentData().toSize();
}

int SettingsDialog::selectedFps() const
{
    return m_fpsSpin->value();
}

void SettingsDialog::onPresetChanged(int index)
{
    Q_UNUSED(index);
    const bool custom = isCustomPreset(m_presetCombo);
    m_customW->setEnabled(custom);
    m_customH->setEnabled(custom);
    if (!custom) {
        const QSize s = m_presetCombo->currentData().toSize();
        m_customW->setValue(s.width());
        m_customH->setValue(s.height());
    }
}

void SettingsDialog::onApplyComposition()
{
    const QSize size = selectedCanvasSize();
    QSettings().setValue(QStringLiteral("composition/width"), size.width());
    QSettings().setValue(QStringLiteral("composition/height"), size.height());
    QSettings().setValue(QStringLiteral("composition/fps"), m_fpsSpin->value());
    QSettings().setValue(QStringLiteral("composition/custom"), isCustomPreset(m_presetCombo));
    emit applyComposition(size, m_fpsSpin->value());
    m_compHint->setText(QStringLiteral("Appliqué : %1 × %2 @ %3 fps")
                            .arg(size.width())
                            .arg(size.height())
                            .arg(m_fpsSpin->value()));
}

void SettingsDialog::onOscToggled(bool enabled)
{
    QSettings().setValue(QStringLiteral("osc/enabled"), enabled);
    if (enabled)
        startOsc();
    else
        stopOsc();
    syncOscUi();
}

void SettingsDialog::startOsc()
{
    if (!m_osc || !m_oscEnable || !m_oscEnable->isChecked())
        return;
    const int port = m_oscPort->value();
    QSettings().setValue(QStringLiteral("osc/port"), port);
    m_osc->start(static_cast<quint16>(port));
    syncOscUi();
}

void SettingsDialog::stopOsc()
{
    if (m_osc)
        m_osc->stop();
    syncOscUi();
}

void SettingsDialog::onMidiToggled(bool enabled)
{
    QSettings().setValue(QStringLiteral("midi/enabled"), enabled);
    if (enabled)
        startMidi();
    else
        stopMidi();
    syncMidiUi();
}

void SettingsDialog::onRefreshMidi()
{
    if (!m_midi)
        return;
    const QSignalBlocker b(m_midiPorts);
    m_midiPorts->clear();
    const QStringList ports = m_midi->availablePorts();
    if (ports.isEmpty()) {
        m_midiPorts->addItem(QStringLiteral("(aucun port)"));
        return;
    }
    m_midiPorts->addItems(ports);
    const int saved = QSettings().value(QStringLiteral("midi/port"), 0).toInt();
    if (saved >= 0 && saved < ports.size())
        m_midiPorts->setCurrentIndex(saved);
}

void SettingsDialog::startMidi()
{
    if (!m_midi || !m_midiEnable || !m_midiEnable->isChecked())
        return;
    if (m_midi->availablePorts().isEmpty()) {
        syncMidiUi();
        return;
    }
    const int idx = m_midiPorts->currentIndex();
    if (idx < 0)
        return;
    m_midi->openPort(idx);
    QSettings().setValue(QStringLiteral("midi/port"), idx);
    syncMidiUi();
}

void SettingsDialog::stopMidi()
{
    if (m_midi)
        m_midi->closePort();
    syncMidiUi();
}

void SettingsDialog::onStartLearn()
{
    if (!m_midi || !m_midiEnable->isChecked() || !m_midi->isOpen())
        return;
    m_midi->setLearnAction(m_learnAction->currentText());
    m_midi->setLearnMode(true);
    if (m_log)
        m_log->appendPlainText(QStringLiteral("Learn: action « %1 » — bouge un contrôleur…")
                                   .arg(m_learnAction->currentText()));
}

void SettingsDialog::onClearBindings()
{
    if (!m_midi)
        return;
    m_midi->clearBindings();
    reloadBindingsList();
}

void SettingsDialog::reloadBindingsList()
{
    m_bindingsList->clear();
    if (!m_midi)
        return;
    for (const MidiBinding &b : m_midi->bindings())
        m_bindingsList->addItem(bindingText(b));
}

void SettingsDialog::syncOscUi()
{
    const bool enabled = m_oscEnable && m_oscEnable->isChecked();
    if (m_oscPort)
        m_oscPort->setEnabled(enabled);
    if (!m_oscStatus)
        return;
    if (m_osc && m_osc->isRunning())
        m_oscStatus->setText(QStringLiteral("OSC: écoute UDP %1").arg(m_osc->port()));
    else if (!enabled)
        m_oscStatus->setText(QStringLiteral("OSC: désactivé"));
    else
        m_oscStatus->setText(QStringLiteral("OSC: arrêté"));
}

void SettingsDialog::syncMidiUi()
{
    const bool enabled = m_midiEnable && m_midiEnable->isChecked();
    if (m_midiPorts)
        m_midiPorts->setEnabled(enabled);
    if (m_learnAction)
        m_learnAction->setEnabled(enabled && m_midi && m_midi->isOpen());
    if (!m_midiStatus)
        return;
    if (!enabled)
        m_midiStatus->setText(QStringLiteral("MIDI: désactivé"));
    else if (m_midi && m_midi->isOpen())
        m_midiStatus->setText(QStringLiteral("MIDI: actif"));
    else
        m_midiStatus->setText(QStringLiteral("MIDI: arrêté"));
}
