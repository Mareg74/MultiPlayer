#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>
#include <vector>

struct MidiBinding
{
    enum class Type { NoteOn, ControlChange };
    Type type = Type::ControlChange;
    int channel = 0; // 0 = any, 1-16 = specific
    int number = 0;  // note or CC number
    QString action;  // e.g. "output", "module.0.play", "module.0.opacity"
};

class MidiController : public QObject
{
    Q_OBJECT

public:
    explicit MidiController(QObject *parent = nullptr);
    ~MidiController() override;

    QStringList availablePorts() const;
    bool openPort(int index);
    void closePort();
    bool isOpen() const { return m_open; }
    int currentPort() const { return m_portIndex; }

    void setLearnMode(bool enabled);
    bool learnMode() const { return m_learnMode; }
    void setLearnAction(const QString &action);

    QVector<MidiBinding> bindings() const { return m_bindings; }
    void setBindings(const QVector<MidiBinding> &bindings);
    void clearBindings();

signals:
    void actionTriggered(const QString &action, float value01);
    void learned(const MidiBinding &binding);
    void statusChanged(const QString &status);

private:
    void onMidi(double /*delta*/, std::vector<unsigned char> *message, void * /*user*/);
    static void rtMidiCallback(double delta, std::vector<unsigned char> *message, void *userData);
    void handleMessage(const std::vector<unsigned char> &message);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_open = false;
    int m_portIndex = -1;
    bool m_learnMode = false;
    QString m_learnAction;
    QVector<MidiBinding> m_bindings;
};
