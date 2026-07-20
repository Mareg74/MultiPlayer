#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>
#include <functional>
#include <memory>

class PlayerModule;
class OutputButton;

// Dispatches OSC addresses and MIDI action strings into app callbacks.
class ControlRouter : public QObject
{
    Q_OBJECT

public:
    explicit ControlRouter(QObject *parent = nullptr);

    void setModules(QVector<std::shared_ptr<PlayerModule>> *modules);
    void setOutputToggle(std::function<void(bool)> fn);
    void setOutputQuery(std::function<bool()> fn);
    void setSelectModule(std::function<void(int)> fn);
    void setTransformNotify(std::function<void()> fn);
    void setPlayAll(std::function<void()> fn);
    void setPauseAll(std::function<void()> fn);
    void setStopAll(std::function<void()> fn);
    void setToggleAll(std::function<void()> fn);

public slots:
    void handleOsc(const QString &address, const QVariantList &args);
    void handleMidiAction(const QString &action, float value01);

signals:
    void logMessage(const QString &message);

private:
    bool parseModuleAddress(const QString &address, int *moduleIndex, QString *command) const;
    void applyModuleCommand(int index, const QString &command, const QVariantList &args);
    void applyNamedAction(const QString &action, float value01);

    QVector<std::shared_ptr<PlayerModule>> *m_modules = nullptr;
    std::function<void(bool)> m_outputToggle;
    std::function<bool()> m_outputQuery;
    std::function<void(int)> m_selectModule;
    std::function<void()> m_transformNotify;
    std::function<void()> m_playAll;
    std::function<void()> m_pauseAll;
    std::function<void()> m_stopAll;
    std::function<void()> m_toggleAll;
};
