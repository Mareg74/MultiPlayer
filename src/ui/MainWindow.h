#pragma once

#include "project/ProjectFile.h"
#include "compose/OutputCanvas.h"

#include <QMainWindow>
#include <QSet>
#include <QSize>
#include <QVector>
#include <memory>

class PlayerModuleWidget;
class OutputButton;
class StatsBar;
class OutputManager;
class MediaBank;
class PlayerModule;
class OutputCanvas;
class PerfMonitor;
class ClipTransformPanel;
class ProgramCanvasWidget;
class OutputMap;
class OscServer;
class MidiController;
class ControlRouter;

class QLabel;
class QListWidget;
class QTimer;
class QKeyEvent;
class QCloseEvent;
class QResizeEvent;
class QPushButton;
class QSlider;
class QGridLayout;
class QWidget;
class QSplitter;
class QGroupBox;
class QButtonGroup;
class QScrollArea;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onOutputToggled(bool onAir);
    void onOpenBankFolder();
    void onBankItemActivated(const QModelIndex &index);
    void onFrameTick();
    void onPerfTick();
    void onLoadProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onOpenRecentProject();
    void rebuildRecentMenu();
    void onSelectModule(int index);
    void onTransformEdited();
    void onSliceLinkChanged(const QString &sliceName, SliceAlignMode mode);
    void onOpenOutputMap();
    void onOpenSettings();
    void applyComposition(QSize size, int fps);
    void onPlayAll();
    void onPauseAll();
    void onStopAll();
    void onToggleAll();
    void onLoadInputGuide();
    void onClearInputGuide();
    void onGuideOpacityChanged(int value);
    void onGuideVisibleToggled(bool visible);
    void onGuideForegroundToggled(bool foreground);
    void onLoadAlphaMask();
    void onClearAlphaMask();
    void onAlphaMaskVisibleToggled(bool visible);
    void onAlphaMaskInvertedToggled(bool inverted);
    void onAlphaMaskOpacityChanged(int value);
    void onAddModule();
    void onRemoveModule();
    void onViewModeChanged();
    void onProgramZoomSlider(int percent);
    void onProgramZoomFit();
    void onProgramZoomOneToOne();
    void onProgramZoomActual();

private:
    void buildUi();
    void applyDarkTheme();
    void rebuildModuleWidgets();
    void wireModules();
    void updateModuleCountButtons();
    void composeAndSend();
    void loadMediaIntoModule(int index, const QString &path);
    void refreshSliceLists();
    bool applyModuleToSlice(int moduleIndex, const QString &sliceName, SliceAlignMode mode,
                            bool warnIfNoMedia);
    void setupControl();
    void saveMidiBindings() const;
    void loadMidiBindings();
    void updateShowStatus();
    void updateGlobalTransportUi();
    void setModulePauseBlink(int index, bool paused);
    void setGlobalPauseBlink(bool active);
    void syncPauseBlinkTimer();
    void onPauseBlinkTick();
    void applyPauseBlinkStyles();
    void updateWindowTitle();
    void markProjectDirty();
    void clearProjectDirty();
    bool trySaveProject();
    void setupAutosave();
    void onAutosaveTick();
    void applyInputGuideToCanvas();
    void applyAlphaMaskUi();
    void applyAlphaMaskToImage(QImage &frame);
    void invalidateAlphaMaskCache();
    bool anyModulePlaying() const;
    QString defaultProjectPath() const;
    InputGuideState currentGuideState() const { return m_inputGuide; }
    AlphaMaskState currentAlphaMaskState() const { return m_alphaMask; }
    void applyModulesViewMode();
    void optimizeProgramPanelLayout();
    int compositionFps() const;
    int modulesColumnCount() const;
    void rememberRecentProject(const QString &path);
    QStringList recentProjects() const;
    bool loadProjectFromPath(const QString &path);

    QVector<PlayerModuleWidget *> m_moduleWidgets;
    QVector<std::shared_ptr<PlayerModule>> m_modules;
    QWidget *m_modulesGridHost = nullptr;
    QGridLayout *m_modulesGrid = nullptr;
    QScrollArea *m_modulesScroll = nullptr;
    QPushButton *m_addModuleBtn = nullptr;
    QPushButton *m_removeModuleBtn = nullptr;
    QPushButton *m_viewVignetteBtn = nullptr;
    QPushButton *m_viewListBtn = nullptr;
    QButtonGroup *m_viewModeGroup = nullptr;
    OutputButton *m_outputButton = nullptr;
    QPushButton *m_globalPlayBtn = nullptr;
    QPushButton *m_globalPauseBtn = nullptr;
    QPushButton *m_globalStopBtn = nullptr;
    StatsBar *m_statsBar = nullptr;
    QListWidget *m_bankList = nullptr;
    QLabel *m_bankPathLabel = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QWidget *m_programPanel = nullptr;
    QGroupBox *m_guideBox = nullptr;
    QGroupBox *m_alphaMaskBox = nullptr;
    ProgramCanvasWidget *m_programCanvas = nullptr;
    ClipTransformPanel *m_clipPanel = nullptr;
    QPushButton *m_guideVisible = nullptr;
    QPushButton *m_guideForeground = nullptr;
    QSlider *m_guideOpacity = nullptr;
    QLabel *m_guidePathLabel = nullptr;
    QPushButton *m_alphaMaskVisible = nullptr;
    QPushButton *m_alphaMaskInvert = nullptr;
    QSlider *m_alphaMaskOpacity = nullptr;
    QLabel *m_alphaMaskPathLabel = nullptr;
    QPushButton *m_zoomFitBtn = nullptr;
    QPushButton *m_zoomOneToOneBtn = nullptr;
    QPushButton *m_zoomActualBtn = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_zoomPercentLabel = nullptr;
    QMenu *m_recentMenu = nullptr;
    QTimer *m_frameTimer = nullptr;
    QTimer *m_perfTimer = nullptr;
    QTimer *m_autosaveTimer = nullptr;
    QTimer *m_pauseBlinkTimer = nullptr;
    QSet<int> m_pauseBlinkModules;
    bool m_globalPauseBlink = false;
    bool m_pauseBlinkPhase = false;
    bool m_listViewMode = false;
    int m_moduleLayoutCols = -1;

    std::unique_ptr<MediaBank> m_bank;
    std::unique_ptr<OutputCanvas> m_canvas;
    std::unique_ptr<OutputManager> m_output;
    std::unique_ptr<OutputMap> m_outputMap;
    std::unique_ptr<PerfMonitor> m_perf;
    std::unique_ptr<OscServer> m_osc;
    std::unique_ptr<MidiController> m_midi;
    std::unique_ptr<ControlRouter> m_router;
    QString m_currentProjectPath;
    InputGuideState m_inputGuide;
    QImage m_guideImage;
    AlphaMaskState m_alphaMask;
    QImage m_alphaMaskImage;
    QImage m_alphaMaskPrepared;
    QSize m_alphaMaskPreparedSize;
    bool m_alphaMaskPreparedInverted = false;
    int m_selectedModule = -1;
    QSize m_lastCanvasSize{1920, 1080};
    bool m_projectDirty = false;
};
