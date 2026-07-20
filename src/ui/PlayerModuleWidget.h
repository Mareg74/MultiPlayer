#pragma once

#include "media/PlayerModule.h"

#include <QFrame>
#include <memory>

class VideoPreviewWidget;
class QPushButton;
class QSlider;
class QLabel;
class QLineEdit;
class QMouseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QMimeData;
class QResizeEvent;

class PlayerModuleWidget : public QFrame
{
    Q_OBJECT

public:
    explicit PlayerModuleWidget(int index, QWidget *parent = nullptr);

    void setModule(const std::shared_ptr<PlayerModule> &module);
    std::shared_ptr<PlayerModule> module() const { return m_module; }
    int index() const { return m_index; }
    void setIndex(int index);
    void setSelected(bool selected);
    void refreshFrame();
    void setPauseBlink(bool active, bool phaseOn);
    void syncOutputVisible();
    void setListMode(bool listMode);

signals:
    void selected(int index);
    void openFileRequested(int index);
    void mediaDropped(int index, const QString &path);
    void projectEdited();
    void pausePressed();
    void playPressed();
    void stopPressed();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onPlayPause();
    void onStop();
    void onLoopToggled(bool checked);
    void onSeek(int value);
    void onOpenFile();
    void onModuleStateChanged();
    void onPositionChanged(qint64 ms, qint64 durationMs);
    void onOutputVisibleToggled(bool visible);
    void onNameEdited();

private:
    int m_index = 0;
    std::shared_ptr<PlayerModule> m_module;
    VideoPreviewWidget *m_preview = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLabel *m_fileLabel = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_loadButton = nullptr;
    QPushButton *m_loopButton = nullptr;
    QPushButton *m_eyeButton = nullptr;
    QSlider *m_seekSlider = nullptr;
    bool m_seekDragging = false;
    bool m_selected = false;
    bool m_dropHighlight = false;
    bool m_pauseBlinkActive = false;
    bool m_pauseBlinkPhase = false;
    bool m_listMode = false;
    QString m_fileFullText;

    void applyChrome();
    void applyPlayButtonStyle();
    void updateEyeIcon();
    void syncNameFromModule();
    void setFileLabelText(const QString &text);
    void updateFileLabelElide();
    static QString mediaPathFromMime(const QMimeData *mime);
};
