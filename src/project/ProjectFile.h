#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <memory>

class PlayerModule;
class MediaBank;
class OutputManager;
class OutputCanvas;
class OutputMap;

struct InputGuideState
{
    QString path;
    float opacity = 0.45f;
    bool visible = true;
    bool foreground = false;
};

struct AlphaMaskState
{
    QString path;
    float opacity = 1.f;
    bool visible = true;
    bool inverted = false;
};

class ProjectFile
{
public:
    static bool save(const QString &path,
                     const QVector<std::shared_ptr<PlayerModule>> &modules,
                     const MediaBank *bank,
                     const OutputCanvas *canvas,
                     const OutputManager *output,
                     const OutputMap *outputMap,
                     int fps,
                     const InputGuideState *guide = nullptr,
                     const AlphaMaskState *alphaMask = nullptr);

    static bool load(const QString &path,
                     QVector<std::shared_ptr<PlayerModule>> &modules,
                     MediaBank *bank,
                     OutputCanvas *canvas,
                     OutputManager *output,
                     OutputMap *outputMap,
                     int *fps,
                     InputGuideState *guide = nullptr,
                     AlphaMaskState *alphaMask = nullptr);
};
