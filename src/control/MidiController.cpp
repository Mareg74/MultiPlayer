#include "control/MidiController.h"

#include <QMetaObject>
#include <RtMidi.h>
#include <vector>

struct MidiController::Impl
{
    std::unique_ptr<RtMidiIn> input;
};

MidiController::MidiController(QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    try {
        m_impl->input = std::make_unique<RtMidiIn>();
    } catch (const RtMidiError &e) {
        emit statusChanged(tr("MIDI: %1").arg(QString::fromStdString(e.getMessage())));
    }
}

MidiController::~MidiController()
{
    closePort();
}

QStringList MidiController::availablePorts() const
{
    QStringList list;
    if (!m_impl->input)
        return list;
    const unsigned int n = m_impl->input->getPortCount();
    for (unsigned int i = 0; i < n; ++i)
        list.append(QString::fromStdString(m_impl->input->getPortName(i)));
    return list;
}

bool MidiController::openPort(int index)
{
    closePort();
    if (!m_impl->input || index < 0)
        return false;
    try {
        if (static_cast<unsigned int>(index) >= m_impl->input->getPortCount())
            return false;
        m_impl->input->openPort(static_cast<unsigned int>(index));
        m_impl->input->setCallback(&MidiController::rtMidiCallback, this);
        m_impl->input->ignoreTypes(false, false, true);
        m_open = true;
        m_portIndex = index;
        emit statusChanged(tr("MIDI: ouvert — %1")
                               .arg(QString::fromStdString(m_impl->input->getPortName(
                                   static_cast<unsigned int>(index)))));
        return true;
    } catch (const RtMidiError &e) {
        emit statusChanged(tr("MIDI: %1").arg(QString::fromStdString(e.getMessage())));
        m_open = false;
        return false;
    }
}

void MidiController::closePort()
{
    if (m_impl->input && m_impl->input->isPortOpen()) {
        m_impl->input->cancelCallback();
        m_impl->input->closePort();
    }
    m_open = false;
    m_portIndex = -1;
    emit statusChanged(tr("MIDI: fermé"));
}

void MidiController::setLearnMode(bool enabled)
{
    m_learnMode = enabled;
}

void MidiController::setLearnAction(const QString &action)
{
    m_learnAction = action;
}

void MidiController::setBindings(const QVector<MidiBinding> &bindings)
{
    m_bindings = bindings;
}

void MidiController::clearBindings()
{
    m_bindings.clear();
}

void MidiController::rtMidiCallback(double delta, std::vector<unsigned char> *message,
                                    void *userData)
{
    auto *self = static_cast<MidiController *>(userData);
    if (self)
        self->onMidi(delta, message, userData);
}

void MidiController::onMidi(double, std::vector<unsigned char> *message, void *)
{
    if (!message || message->empty())
        return;
    // RtMidi callback is on MIDI thread — hop to Qt thread
    std::vector<unsigned char> copy = *message;
    QMetaObject::invokeMethod(
        this,
        [this, copy]() { handleMessage(copy); },
        Qt::QueuedConnection);
}

void MidiController::handleMessage(const std::vector<unsigned char> &message)
{
    if (message.empty())
        return;
    const unsigned char status = message[0];
    const unsigned char type = status & 0xF0;
    const int channel = (status & 0x0F) + 1;

    MidiBinding::Type bindType;
    int number = 0;
    float value01 = 0.f;

    if (type == 0x90 && message.size() >= 3) {
        // Note on
        number = message[1];
        const int vel = message[2];
        if (vel == 0)
            return; // note off as note-on vel 0
        bindType = MidiBinding::Type::NoteOn;
        value01 = vel / 127.f;
    } else if (type == 0xB0 && message.size() >= 3) {
        bindType = MidiBinding::Type::ControlChange;
        number = message[1];
        value01 = message[2] / 127.f;
    } else {
        return;
    }

    if (m_learnMode && !m_learnAction.isEmpty()) {
        MidiBinding b;
        b.type = bindType;
        b.channel = channel;
        b.number = number;
        b.action = m_learnAction;
        // Replace existing same action
        for (int i = 0; i < m_bindings.size(); ++i) {
            if (m_bindings[i].action == b.action) {
                m_bindings[i] = b;
                emit learned(b);
                m_learnMode = false;
                return;
            }
        }
        m_bindings.push_back(b);
        emit learned(b);
        m_learnMode = false;
        return;
    }

    for (const MidiBinding &b : m_bindings) {
        if (b.type != bindType)
            continue;
        if (b.number != number)
            continue;
        if (b.channel != 0 && b.channel != channel)
            continue;
        emit actionTriggered(b.action, value01);
    }
}
