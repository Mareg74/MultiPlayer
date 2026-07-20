#include "ui/MainWindow.h"
#include "ui/ClipTransformPanel.h"
#include "ui/OutputButton.h"
#include "ui/PlayerModuleWidget.h"
#include "ui/ProgramCanvasWidget.h"
#include "ui/StatsBar.h"

#include "compose/OutputCanvas.h"
#include "control/ControlRouter.h"
#include "control/MidiController.h"
#include "control/OscServer.h"
#include "core/PerfMonitor.h"
#include "map/OutputMap.h"
#include "media/MediaBank.h"
#include "media/PlayerModule.h"
#include "output/OutputManager.h"
#include "project/ProjectFile.h"
#include "ui/OutputMapDialog.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

class MediaBankListWidget : public QListWidget
{
public:
    explicit MediaBankListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setToolTip(QStringLiteral("Glisser vers un module pour charger le média"));
    }

protected:
    QStringList mimeTypes() const override
    {
        return {QStringLiteral("text/uri-list")};
    }

    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        auto *mime = new QMimeData;
        QList<QUrl> urls;
        for (const QListWidgetItem *item : items) {
            if (!item)
                continue;
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                urls.append(QUrl::fromLocalFile(path));
        }
        mime->setUrls(urls);
        return mime;
    }
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_bank(std::make_unique<MediaBank>())
    , m_canvas(std::make_unique<OutputCanvas>())
    , m_output(std::make_unique<OutputManager>())
    , m_outputMap(std::make_unique<OutputMap>())
    , m_perf(std::make_unique<PerfMonitor>())
    , m_osc(std::make_unique<OscServer>())
    , m_midi(std::make_unique<MidiController>())
    , m_router(std::make_unique<ControlRouter>())
{
    setWindowTitle(QStringLiteral("%1 — %2").arg(QStringLiteral(MP_APP_NAME),
                                                 QStringLiteral(MP_APP_ID)));
    resize(1500, 940);

    for (int i = 0; i < MP_MODULE_DEFAULT; ++i)
        m_modules.push_back(std::make_shared<PlayerModule>(i));

    m_canvas->setSize(1920, 1080);
    m_output->setResolution(1920, 1080);
    m_outputMap->resetFullscreen(1920, 1080);
    m_lastCanvasSize = QSize(1920, 1080);

    applyDarkTheme();
    buildUi();
    rebuildModuleWidgets();
    onSelectModule(-1);
    setupControl();
    optimizeProgramPanelLayout();
    QTimer::singleShot(0, this, [this]() {
        m_moduleLayoutCols = -1;
        applyModulesViewMode();
    });

    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, &QTimer::timeout, this, &MainWindow::onFrameTick);
    m_frameTimer->start(33);

    m_perfTimer = new QTimer(this);
    connect(m_perfTimer, &QTimer::timeout, this, &MainWindow::onPerfTick);
    m_perfTimer->start(1000);

    setupAutosave();

    QSettings settings;
    const QString last = settings.value(QStringLiteral("lastProject")).toString();
    if (!last.isEmpty() && QFileInfo::exists(last)) {
        int fps = 30;
        InputGuideState guide;
        AlphaMaskState alphaMask;
        if (ProjectFile::load(last, m_modules, m_bank.get(), m_canvas.get(), m_output.get(),
                              m_outputMap.get(), &fps, &guide, &alphaMask)) {
            m_currentProjectPath = last;
            m_output->setFps(fps);
            m_frameTimer->setInterval(qMax(1, 1000 / qMax(1, fps)));
            m_lastCanvasSize = m_canvas->size();
            m_outputMap->matchCanvas(m_lastCanvasSize.width(), m_lastCanvasSize.height());
            m_programCanvas->setCanvasSize(m_lastCanvasSize.width(), m_lastCanvasSize.height());
            updateShowStatus();
            m_inputGuide = guide;
            m_guideImage = QImage();
            applyInputGuideToCanvas();
            m_alphaMask = alphaMask;
            m_alphaMaskImage = QImage();
            invalidateAlphaMaskCache();
            applyAlphaMaskUi();
            rebuildModuleWidgets();
            onSelectModule(-1);
            rememberRecentProject(last);
            refreshSliceLists();
        }
    }
    clearProjectDirty();
}

MainWindow::~MainWindow()
{
    if (m_output)
        m_output->stop();
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget {"
        "  background-color: #0b0d11;"
        "  color: #d5dbe6;"
        "  font-family: 'Avenir Next', 'Helvetica Neue', sans-serif;"
        "  font-size: 13px;"
        "}"
        "QMenuBar { background: #10141a; padding: 2px; }"
        "QMenuBar::item { padding: 6px 10px; }"
        "QMenuBar::item:selected { background: #1c2533; }"
        "QMenu { background: #121820; border: 1px solid #2a3340; }"
        "QMenu::item:selected { background: #243044; }"
        "QGroupBox {"
        "  border: 1px solid #232b36;"
        "  margin-top: 12px;"
        "  padding: 10px 8px 8px 8px;"
        "  background: #0f1319;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin; left: 10px;"
        "  color: #7f8b9c; font-size: 11px; font-weight: 600;"
        "  letter-spacing: 0.6px; text-transform: uppercase;"
        "}"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QListWidget, QPlainTextEdit {"
        "  background: #141a22; border: 1px solid #2a3340; border-radius: 2px; padding: 5px;"
        "  selection-background-color: #2f5f9a;"
        "}"
        "QListWidget::item { padding: 4px 6px; }"
        "QListWidget::item:selected { background: #24344a; color: #f0f4fa; }"
        "QPushButton {"
        "  background: #1a222d; border: 1px solid #303848; border-radius: 2px;"
        "  padding: 5px 10px; font-weight: 600;"
        "}"
        "QPushButton:hover { background: #243040; }"
        "QPushButton:pressed { background: #151b24; }"
        "QStatusBar { background: #0a0c10; color: #8b93a3; border-top: 1px solid #1a212b; }"
        "QSplitter::handle { background: #161c24; width: 3px; }"
        "QSlider::groove:horizontal { height: 3px; background: #2a3340; }"
        "QSlider::handle:horizontal {"
        "  width: 11px; margin: -5px 0; background: #e8ecf2; border-radius: 1px;"
        "}"
        "QCheckBox { spacing: 6px; }"
        "QTabWidget::pane { border: 1px solid #232b36; background: #0f1319; }"
        "QTabBar::tab {"
        "  background: #141a22; color: #8b95a8; padding: 8px 14px;"
        "  border: 1px solid #232b36; margin-right: 2px;"
        "}"
        "QTabBar::tab:selected { background: #1a2430; color: #f0f4fa; border-bottom-color: #1a2430; }"
        ));
}

void MainWindow::buildUi()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("Fichier"));
    fileMenu->addAction(QStringLiteral("Ouvrir projet…"), this, &MainWindow::onLoadProject);
    m_recentMenu = fileMenu->addMenu(QStringLiteral("Ouvrir récent"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentMenu);
    rebuildRecentMenu();
    fileMenu->addAction(QStringLiteral("Enregistrer"), this, &MainWindow::onSaveProject);
    fileMenu->addAction(QStringLiteral("Enregistrer sous…"), this, &MainWindow::onSaveProjectAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Quitter"), this, &QWidget::close);

    auto *mapMenu = menuBar()->addMenu(QStringLiteral("Output"));
    mapMenu->addAction(QStringLiteral("Output Map…"), this, &MainWindow::onOpenOutputMap);

    auto *settingsMenu = menuBar()->addMenu(QStringLiteral("Paramètres"));
    settingsMenu->addAction(QStringLiteral("Paramètres…"), QKeySequence::Preferences, this,
                            &MainWindow::onOpenSettings);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(10, 8, 10, 6);
    root->setSpacing(8);

    // Top strip — horizontally scrollable if the window is too narrow
    auto *topScroll = new QScrollArea(this);
    topScroll->setWidgetResizable(true);
    topScroll->setFrameShape(QFrame::NoFrame);
    topScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    topScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    topScroll->setMaximumHeight(64);
    topScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *topHost = new QWidget(topScroll);
    auto *top = new QHBoxLayout(topHost);
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(12);
    auto *title = new QLabel(QStringLiteral(MP_APP_NAME), topHost);
    title->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 800; letter-spacing: 1px; color: #f4f7fb;"));
    top->addWidget(title);

    auto *openSettingsBtn = new QPushButton(QStringLiteral("Paramètres"), topHost);
    openSettingsBtn->setToolTip(QStringLiteral("Composition, sorties NDI/Spout, OSC, MIDI"));
    top->addWidget(openSettingsBtn);
    top->addStretch(1);

    // View mode (à droite, avant le transport — zone jaune)
    auto *viewBox = new QWidget(topHost);
    auto *viewLay = new QHBoxLayout(viewBox);
    viewLay->setContentsMargins(0, 0, 0, 0);
    viewLay->setSpacing(0);
    m_viewVignetteBtn = new QPushButton(QStringLiteral("Vignettes"), viewBox);
    m_viewListBtn = new QPushButton(QStringLiteral("Lignes"), viewBox);
    m_viewVignetteBtn->setCheckable(true);
    m_viewListBtn->setCheckable(true);
    m_viewVignetteBtn->setChecked(true);
    m_viewVignetteBtn->setToolTip(QStringLiteral("Grille avec preview par module"));
    m_viewListBtn->setToolTip(QStringLiteral("Liste compacte sans preview (moins de charge GPU/CPU)"));
    const QString viewBtnStyle = QStringLiteral(
        "QPushButton {"
        "  padding: 6px 14px; font-weight: 700; font-size: 11px;"
        "  background: #141a22; color: #8b95a8; border: 1px solid #2a3340;"
        "}"
        "QPushButton:checked {"
        "  background: #1e3a5f; color: #e8eef8; border-color: #4f9cff;"
        "}"
        "QPushButton:hover { background: #1a2430; color: #c5cedd; }");
    m_viewVignetteBtn->setStyleSheet(viewBtnStyle
                                     + QStringLiteral("QPushButton { border-top-left-radius: 3px;"
                                                      " border-bottom-left-radius: 3px; }"));
    m_viewListBtn->setStyleSheet(viewBtnStyle
                                 + QStringLiteral("QPushButton { border-top-right-radius: 3px;"
                                                  " border-bottom-right-radius: 3px;"
                                                  " border-left: none; }"));
    m_viewModeGroup = new QButtonGroup(this);
    m_viewModeGroup->setExclusive(true);
    m_viewModeGroup->addButton(m_viewVignetteBtn, 0);
    m_viewModeGroup->addButton(m_viewListBtn, 1);
    viewLay->addWidget(m_viewVignetteBtn);
    viewLay->addWidget(m_viewListBtn);
    top->addWidget(viewBox);

    m_listViewMode = QSettings().value(QStringLiteral("ui/listView"), false).toBool();
    m_viewVignetteBtn->setChecked(!m_listViewMode);
    m_viewListBtn->setChecked(m_listViewMode);

    // Global transport
    auto *transport = new QWidget(topHost);
    auto *transportLayout = new QHBoxLayout(transport);
    transportLayout->setContentsMargins(0, 0, 0, 0);
    transportLayout->setSpacing(4);
    m_globalPlayBtn = new QPushButton(QStringLiteral("▶ ALL"), transport);
    m_globalPauseBtn = new QPushButton(QStringLiteral("❚❚ ALL"), transport);
    m_globalStopBtn = new QPushButton(QStringLiteral("■ ALL"), transport);
    m_globalPlayBtn->setToolTip(QStringLiteral("Lecture de tous les modules (Shift+Space)"));
    m_globalPauseBtn->setToolTip(QStringLiteral("Pause de tous les modules"));
    m_globalStopBtn->setToolTip(QStringLiteral("Stop de tous les modules (Shift+S)"));
    for (QPushButton *b : {m_globalPlayBtn, m_globalPauseBtn, m_globalStopBtn}) {
        b->setMinimumHeight(36);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { font-weight: 800; letter-spacing: 0.5px; min-width: 64px; }"));
    }
    transportLayout->addWidget(m_globalPlayBtn);
    transportLayout->addWidget(m_globalPauseBtn);
    transportLayout->addWidget(m_globalStopBtn);
    top->addWidget(transport);

    m_outputButton = new OutputButton(topHost);
    m_outputButton->setToolTip(QStringLiteral("Basculer la sortie Program (raccourci O)"));
    top->addWidget(m_outputButton);
    topHost->setMinimumWidth(720);
    topScroll->setWidget(topHost);
    root->addWidget(topScroll);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(4);
    auto *splitter = m_mainSplitter;

    // LEFT — media + clip (scroll when window is too short)
    auto *leftScroll = new QScrollArea(this);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->setMinimumWidth(200);
    leftScroll->setMaximumWidth(360);

    auto *left = new QWidget(leftScroll);
    left->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(8);

    m_clipPanel = new ClipTransformPanel(left);
    m_clipPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    leftLayout->addWidget(m_clipPanel);
    leftLayout->addStretch(1);

    leftScroll->setWidget(left);
    splitter->addWidget(leftScroll);

    // CENTER — modules
    auto *center = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(4, 0, 4, 0);
    centerLayout->setSpacing(4);
    auto *modulesHeader = new QLabel(
        QStringLiteral("MODULES  ·  Space = module  ·  Shift+Space = ALL  ·  S / Shift+S stop  ·  1–9 select"),
        center);
    modulesHeader->setStyleSheet(QStringLiteral(
        "color: #6f7a8a; font-size: 11px; letter-spacing: 0.5px; font-weight: 600;"));
    m_addModuleBtn = new QPushButton(QStringLiteral("+ Player"), center);
    m_addModuleBtn->setToolTip(QStringLiteral("Ajouter un player (max %1)").arg(MP_MODULE_MAX));
    m_addModuleBtn->setFixedHeight(24);
    m_removeModuleBtn = new QPushButton(QStringLiteral("− Player"), center);
    m_removeModuleBtn->setToolTip(QStringLiteral("Supprimer le player sélectionné (min 1)"));
    m_removeModuleBtn->setFixedHeight(24);
    auto *modulesTop = new QHBoxLayout();
    modulesTop->setSpacing(6);
    modulesTop->addWidget(modulesHeader, 1);
    modulesTop->addWidget(m_addModuleBtn);
    modulesTop->addWidget(m_removeModuleBtn);
    centerLayout->addLayout(modulesTop);

    m_modulesScroll = new QScrollArea(center);
    m_modulesScroll->setWidgetResizable(true);
    m_modulesScroll->setFrameShape(QFrame::NoFrame);
    m_modulesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_modulesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_modulesScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_modulesGridHost = new QWidget();
    m_modulesGridHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    m_modulesGrid = new QGridLayout(m_modulesGridHost);
    m_modulesGrid->setContentsMargins(0, 0, 0, 0);
    m_modulesGrid->setSpacing(5);
    m_modulesGrid->setSizeConstraint(QLayout::SetDefaultConstraint);
    m_modulesScroll->setWidget(m_modulesGridHost);
    centerLayout->addWidget(m_modulesScroll, 1);
    splitter->addWidget(center);

    connect(m_addModuleBtn, &QPushButton::clicked, this, &MainWindow::onAddModule);
    connect(m_removeModuleBtn, &QPushButton::clicked, this, &MainWindow::onRemoveModule);
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        applyModulesViewMode();
    });

    // RIGHT — program (scrollable when content exceeds panel)
    auto *rightScroll = new QScrollArea(this);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rightScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rightScroll->setMinimumWidth(200);

    m_programPanel = new QWidget();
    m_programPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *rightLayout = new QVBoxLayout(m_programPanel);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(4);
    auto *progHeader = new QLabel(QStringLiteral("PROGRAM"), m_programPanel);
    progHeader->setStyleSheet(QStringLiteral(
        "color: #6f7a8a; font-size: 11px; letter-spacing: 1px; font-weight: 700;"));
    auto *progHint = new QLabel(QStringLiteral("Snap · Alt = free · Molette = scale"), m_programPanel);
    progHint->setStyleSheet(QStringLiteral("color: #5a6472; font-size: 11px;"));
    auto *progTop = new QHBoxLayout();
    progTop->addWidget(progHeader);
    progTop->addStretch();
    progTop->addWidget(progHint);
    rightLayout->addLayout(progTop);

    m_guideBox = new QGroupBox(QStringLiteral("Input guide"), m_programPanel);
    auto *guideOuter = new QVBoxLayout(m_guideBox);
    guideOuter->setContentsMargins(8, 10, 8, 8);
    guideOuter->setSpacing(6);

    const QString toggleBtnStyle = QStringLiteral(
        "QPushButton {"
        "  padding: 5px 12px; font-weight: 700; font-size: 11px;"
        "  background: #141a22; color: #8b95a8; border: 1px solid #2a3340;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:checked {"
        "  background: #1e3a5f; color: #e8eef8; border-color: #4f9cff;"
        "}"
        "QPushButton:hover { background: #1a2430; color: #c5cedd; }");

    auto *guideRow1 = new QHBoxLayout();
    guideRow1->setSpacing(6);
    m_guideVisible = new QPushButton(QStringLiteral("Visible"), m_guideBox);
    m_guideVisible->setCheckable(true);
    m_guideVisible->setChecked(true);
    m_guideVisible->setStyleSheet(toggleBtnStyle);
    m_guideVisible->setToolTip(QStringLiteral(
        "Affiche le guide sur le Program (pas envoyé en NDI/Spout)"));
    m_guideForeground = new QPushButton(QStringLiteral("1er plan"), m_guideBox);
    m_guideForeground->setCheckable(true);
    m_guideForeground->setChecked(false);
    m_guideForeground->setStyleSheet(toggleBtnStyle);
    m_guideForeground->setToolTip(
        QStringLiteral("Place le guide au-dessus de la composition (Program uniquement)"));
    auto *guideLoad = new QPushButton(QStringLiteral("Charger…"), m_guideBox);
    auto *guideClear = new QPushButton(QStringLiteral("Effacer"), m_guideBox);
    for (QPushButton *b : {guideLoad, guideClear}) {
        b->setFixedHeight(26);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { padding: 4px 10px; font-size: 11px; font-weight: 600; }"));
    }
    guideRow1->addWidget(m_guideVisible);
    guideRow1->addWidget(m_guideForeground);
    guideRow1->addStretch(1);
    guideRow1->addWidget(guideLoad);
    guideRow1->addWidget(guideClear);

    auto *guideRow2 = new QHBoxLayout();
    guideRow2->setSpacing(6);
    auto *opLabel = new QLabel(QStringLiteral("Opacité"), m_guideBox);
    opLabel->setStyleSheet(QStringLiteral("color: #9aa5b5; font-size: 11px;"));
    m_guideOpacity = new QSlider(Qt::Horizontal, m_guideBox);
    m_guideOpacity->setRange(0, 100);
    m_guideOpacity->setValue(45);
    m_guideOpacity->setMinimumWidth(80);
    m_guideOpacity->setToolTip(QStringLiteral("Opacité du guide"));
    m_guidePathLabel = new QLabel(QStringLiteral("—"), m_guideBox);
    m_guidePathLabel->setStyleSheet(QStringLiteral("color: #6f7a8a; font-size: 11px;"));
    m_guidePathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_guidePathLabel->setMinimumWidth(0);
    m_guidePathLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    guideRow2->addWidget(opLabel);
    guideRow2->addWidget(m_guideOpacity, 2);
    guideRow2->addWidget(m_guidePathLabel, 3);

    guideOuter->addLayout(guideRow1);
    guideOuter->addLayout(guideRow2);
    rightLayout->addWidget(m_guideBox);

    // Masque alpha PNG (overlay devant tous les clips)
    m_alphaMaskBox = new QGroupBox(QStringLiteral("Masque alpha (PNG)"), m_programPanel);
    auto *maskOuter = new QVBoxLayout(m_alphaMaskBox);
    maskOuter->setContentsMargins(8, 10, 8, 8);
    maskOuter->setSpacing(6);
    auto *maskRow = new QHBoxLayout();
    maskRow->setSpacing(6);
    m_alphaMaskVisible = new QPushButton(QStringLiteral("Visible"), m_alphaMaskBox);
    m_alphaMaskVisible->setCheckable(true);
    m_alphaMaskVisible->setChecked(true);
    m_alphaMaskVisible->setStyleSheet(toggleBtnStyle);
    m_alphaMaskVisible->setToolTip(
        QStringLiteral("Affiche le PNG devant tous les clips (Program + NDI/Spout).\n"
                       "Les zones transparentes laissent voir la composition."));
    m_alphaMaskInvert = new QPushButton(QStringLiteral("Inverser"), m_alphaMaskBox);
    m_alphaMaskInvert->setCheckable(true);
    m_alphaMaskInvert->setChecked(false);
    m_alphaMaskInvert->setStyleSheet(toggleBtnStyle);
    m_alphaMaskInvert->setToolTip(
        QStringLiteral("Inverse l’alpha du PNG (zones opaques ↔ transparentes)"));
    auto *maskLoad = new QPushButton(QStringLiteral("Charger…"), m_alphaMaskBox);
    auto *maskClear = new QPushButton(QStringLiteral("Effacer"), m_alphaMaskBox);
    for (QPushButton *b : {maskLoad, maskClear}) {
        b->setFixedHeight(26);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { padding: 4px 10px; font-size: 11px; font-weight: 600; }"));
    }
    m_alphaMaskPathLabel = new QLabel(QStringLiteral("—"), m_alphaMaskBox);
    m_alphaMaskPathLabel->setStyleSheet(QStringLiteral("color: #6f7a8a; font-size: 11px;"));
    m_alphaMaskPathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_alphaMaskPathLabel->setMinimumWidth(0);
    maskRow->addWidget(m_alphaMaskVisible);
    maskRow->addWidget(m_alphaMaskInvert);
    maskRow->addWidget(maskLoad);
    maskRow->addWidget(maskClear);
    maskRow->addWidget(m_alphaMaskPathLabel, 1);

    auto *maskRow2 = new QHBoxLayout();
    maskRow2->setSpacing(6);
    auto *maskOpLabel = new QLabel(QStringLiteral("Opacité"), m_alphaMaskBox);
    maskOpLabel->setStyleSheet(QStringLiteral("color: #9aa5b5; font-size: 11px;"));
    m_alphaMaskOpacity = new QSlider(Qt::Horizontal, m_alphaMaskBox);
    m_alphaMaskOpacity->setRange(0, 100);
    m_alphaMaskOpacity->setValue(100);
    m_alphaMaskOpacity->setMinimumWidth(80);
    m_alphaMaskOpacity->setToolTip(
        QStringLiteral("Transparence du masque : 0 % = invisible, 100 % = opaque"));
    maskRow2->addWidget(maskOpLabel);
    maskRow2->addWidget(m_alphaMaskOpacity, 1);

    maskOuter->addLayout(maskRow);
    maskOuter->addLayout(maskRow2);
    rightLayout->addWidget(m_alphaMaskBox);

    // Zoom bar
    auto *zoomBar = new QWidget(m_programPanel);
    auto *zoomLay = new QHBoxLayout(zoomBar);
    zoomLay->setContentsMargins(0, 0, 0, 0);
    zoomLay->setSpacing(6);
    m_zoomFitBtn = new QPushButton(QStringLiteral("Fit"), zoomBar);
    m_zoomOneToOneBtn = new QPushButton(QStringLiteral("1:1"), zoomBar);
    m_zoomActualBtn = new QPushButton(QStringLiteral("Taille réelle"), zoomBar);
    for (QPushButton *b : {m_zoomFitBtn, m_zoomOneToOneBtn, m_zoomActualBtn}) {
        b->setFixedHeight(22);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { padding: 2px 8px; font-size: 11px; font-weight: 600; }"));
    }
    m_zoomFitBtn->setToolTip(QStringLiteral("Ajuster la composition à la zone Program"));
    m_zoomOneToOneBtn->setToolTip(QStringLiteral("1 pixel composition = 1 pixel écran"));
    m_zoomActualBtn->setToolTip(QStringLiteral("Taille réelle (1:1)"));
    m_zoomPercentLabel = new QLabel(QStringLiteral("100%"), zoomBar);
    m_zoomPercentLabel->setMinimumWidth(42);
    m_zoomPercentLabel->setStyleSheet(QStringLiteral("color: #9aa5b5; font-size: 11px;"));
    m_zoomSlider = new QSlider(Qt::Horizontal, zoomBar);
    m_zoomSlider->setRange(10, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(110);
    m_zoomSlider->setToolTip(QStringLiteral("Zoom de la preview Program"));
    zoomLay->addWidget(m_zoomFitBtn);
    zoomLay->addWidget(m_zoomOneToOneBtn);
    zoomLay->addWidget(m_zoomActualBtn);
    zoomLay->addWidget(m_zoomPercentLabel);
    zoomLay->addWidget(m_zoomSlider, 1);
    rightLayout->addWidget(zoomBar);

    m_programCanvas = new ProgramCanvasWidget(m_programPanel);
    m_programCanvas->setModules(&m_modules);
    m_programCanvas->setOutputMap(m_outputMap.get());
    m_programCanvas->setCanvasSize(1920, 1080);
    m_programCanvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_programCanvas->setMinimumSize(200, 150);
    rightLayout->addWidget(m_programCanvas, 1);

    // Banque sous le Program (zone orange)
    auto *bankBox = new QGroupBox(QStringLiteral("Banque"), m_programPanel);
    auto *bankLayout = new QVBoxLayout(bankBox);
    bankLayout->setSpacing(4);
    bankLayout->setContentsMargins(6, 8, 6, 6);
    m_bankPathLabel = new QLabel(QStringLiteral("Aucun dossier"), bankBox);
    m_bankPathLabel->setWordWrap(true);
    m_bankPathLabel->setStyleSheet(QStringLiteral("color: #7a8494; font-size: 11px;"));
    auto *openBankBtn = new QPushButton(QStringLiteral("Dossier…"), bankBox);
    m_bankList = new MediaBankListWidget(bankBox);
    m_bankList->setAlternatingRowColors(false);
    m_bankList->setMinimumHeight(100);
    m_bankList->setMaximumHeight(160);
    m_bankList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    bankLayout->addWidget(m_bankPathLabel);
    bankLayout->addWidget(openBankBtn);
    bankLayout->addWidget(m_bankList, 1);
    rightLayout->addWidget(bankBox);

    rightScroll->setWidget(m_programPanel);
    splitter->addWidget(rightScroll);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 2);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setCollapsible(2, false);
    root->addWidget(splitter, 1);

    m_statsBar = new StatsBar(this);
    statusBar()->addWidget(m_statsBar, 1);
    updateShowStatus();

    connect(openBankBtn, &QPushButton::clicked, this, &MainWindow::onOpenBankFolder);
    connect(m_bankList, &QListWidget::doubleClicked, this, &MainWindow::onBankItemActivated);
    connect(m_outputButton, &OutputButton::outputToggled, this, &MainWindow::onOutputToggled);
    connect(m_globalPlayBtn, &QPushButton::clicked, this, &MainWindow::onPlayAll);
    connect(m_globalPauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseAll);
    connect(m_globalStopBtn, &QPushButton::clicked, this, &MainWindow::onStopAll);
    connect(openSettingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
    connect(m_viewModeGroup, &QButtonGroup::idClicked, this, [this](int) { onViewModeChanged(); });
    connect(m_clipPanel, &ClipTransformPanel::transformChanged, this, &MainWindow::onTransformEdited);
    connect(m_clipPanel, &ClipTransformPanel::sliceLinkChanged, this,
            &MainWindow::onSliceLinkChanged);
    connect(m_programCanvas, &ProgramCanvasWidget::moduleSelected, this, &MainWindow::onSelectModule);
    connect(m_programCanvas, &ProgramCanvasWidget::moduleMoved, this, [this](int) {
        m_clipPanel->reloadFromModule();
        markProjectDirty();
    });
    connect(m_programCanvas, &ProgramCanvasWidget::viewZoomChanged, this, [this](int percent) {
        if (m_zoomSlider) {
            const QSignalBlocker b(m_zoomSlider);
            m_zoomSlider->setValue(percent);
        }
        if (m_zoomPercentLabel)
            m_zoomPercentLabel->setText(QStringLiteral("%1%").arg(percent));
    });
    connect(m_zoomSlider, &QSlider::valueChanged, this, &MainWindow::onProgramZoomSlider);
    connect(m_zoomFitBtn, &QPushButton::clicked, this, &MainWindow::onProgramZoomFit);
    connect(m_zoomOneToOneBtn, &QPushButton::clicked, this, &MainWindow::onProgramZoomOneToOne);
    connect(m_zoomActualBtn, &QPushButton::clicked, this, &MainWindow::onProgramZoomActual);
    connect(guideLoad, &QPushButton::clicked, this, &MainWindow::onLoadInputGuide);
    connect(guideClear, &QPushButton::clicked, this, &MainWindow::onClearInputGuide);
    connect(m_guideOpacity, &QSlider::valueChanged, this, &MainWindow::onGuideOpacityChanged);
    connect(m_guideVisible, &QPushButton::toggled, this, &MainWindow::onGuideVisibleToggled);
    connect(m_guideForeground, &QPushButton::toggled, this, &MainWindow::onGuideForegroundToggled);
    connect(maskLoad, &QPushButton::clicked, this, &MainWindow::onLoadAlphaMask);
    connect(maskClear, &QPushButton::clicked, this, &MainWindow::onClearAlphaMask);
    connect(m_alphaMaskVisible, &QPushButton::toggled, this,
            &MainWindow::onAlphaMaskVisibleToggled);
    connect(m_alphaMaskInvert, &QPushButton::toggled, this,
            &MainWindow::onAlphaMaskInvertedToggled);
    connect(m_alphaMaskOpacity, &QSlider::valueChanged, this,
            &MainWindow::onAlphaMaskOpacityChanged);
    connect(m_bank.get(), &MediaBank::changed, this, [this]() {
        m_bankList->clear();
        m_bankPathLabel->setText(m_bank->folder().isEmpty() ? QStringLiteral("Aucun dossier")
                                                            : m_bank->folder());
        for (const QString &f : m_bank->files()) {
            auto *item = new QListWidgetItem(QFileInfo(f).fileName());
            item->setData(Qt::UserRole, f);
            item->setToolTip(f);
            m_bankList->addItem(item);
        }
    });
}

void MainWindow::rebuildModuleWidgets()
{
    for (auto &mod : m_modules) {
        if (mod)
            disconnect(mod.get(), &PlayerModule::frameUpdated, this, nullptr);
    }

    qDeleteAll(m_moduleWidgets);
    m_moduleWidgets.clear();
    m_pauseBlinkModules.clear();
    m_globalPauseBlink = false;
    syncPauseBlinkTimer();

    if (m_modulesGrid) {
        while (m_modulesGrid->count() > 0) {
            QLayoutItem *item = m_modulesGrid->takeAt(0);
            delete item;
        }
    }

    const int n = m_modules.size();
    m_moduleLayoutCols = -1; // force reflow
    const int cols = modulesColumnCount();
    m_moduleLayoutCols = cols;
    for (int c = 0; c < 4; ++c)
        m_modulesGrid->setColumnStretch(c, c < cols ? 1 : 0);
    for (int i = 0; i < n; ++i) {
        if (m_modules[i])
            m_modules[i]->setIndex(i);
        auto *w = new PlayerModuleWidget(i, m_modulesGridHost);
        // Compact list row when forced to 1 column by narrow width, or user chose Lignes
        w->setListMode(m_listViewMode || cols <= 1);
        m_moduleWidgets.push_back(w);
        m_modulesGrid->addWidget(w, i / cols, i % cols);
    }

    wireModules();
    updateModuleCountButtons();
    applyPauseBlinkStyles();
}

int MainWindow::modulesColumnCount() const
{
    const int n = m_moduleWidgets.isEmpty() ? m_modules.size() : m_moduleWidgets.size();
    if (n <= 0)
        return 1;
    if (m_listViewMode)
        return 1;

    int avail = 400;
    if (m_modulesScroll && m_modulesScroll->viewport())
        avail = m_modulesScroll->viewport()->width();
    // Leave a little margin; below ~220px only one column fits → vertical list
    constexpr int minColWidth = 200;
    const int spacing = m_modulesGrid ? m_modulesGrid->spacing() : 5;
    int cols = (avail + spacing) / (minColWidth + spacing);
    cols = qBound(1, cols, 4);
    return qMin(cols, n);
}

void MainWindow::onViewModeChanged()
{
    m_listViewMode = m_viewListBtn && m_viewListBtn->isChecked();
    QSettings().setValue(QStringLiteral("ui/listView"), m_listViewMode);
    m_moduleLayoutCols = -1;
    applyModulesViewMode();
}

void MainWindow::applyModulesViewMode()
{
    const int n = m_moduleWidgets.size();
    if (n == 0 || !m_modulesGrid)
        return;

    const int cols = modulesColumnCount();
    const bool useListRows = m_listViewMode || cols <= 1;
    if (cols == m_moduleLayoutCols) {
        // Still refresh list/vignette chrome if needed
        for (auto *w : m_moduleWidgets) {
            if (w)
                w->setListMode(useListRows);
        }
        return;
    }
    m_moduleLayoutCols = cols;

    for (auto *w : m_moduleWidgets) {
        if (w)
            m_modulesGrid->removeWidget(w);
    }
    for (int c = 0; c < 4; ++c)
        m_modulesGrid->setColumnStretch(c, c < cols ? 1 : 0);
    for (int i = 0; i < n; ++i) {
        auto *w = m_moduleWidgets[i];
        if (!w)
            continue;
        w->setListMode(useListRows);
        m_modulesGrid->addWidget(w, i / cols, i % cols);
    }
    if (m_modulesGridHost)
        m_modulesGridHost->updateGeometry();
}

void MainWindow::optimizeProgramPanelLayout()
{
    if (!m_programPanel || !m_mainSplitter)
        return;
    const int w = width();
    const int h = height();
    const int progMin = w < 1100 ? 200 : (w < 1400 ? 260 : 320);
    if (auto *scroll = qobject_cast<QScrollArea *>(m_mainSplitter->widget(2)))
        scroll->setMinimumWidth(progMin);
    if (m_programCanvas) {
        const int canvasMinH = qMax(150, h / 3);
        m_programCanvas->setMinimumSize(200, canvasMinH);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    optimizeProgramPanelLayout();
    applyModulesViewMode();
}

void MainWindow::updateModuleCountButtons()
{
    if (m_addModuleBtn)
        m_addModuleBtn->setEnabled(m_modules.size() < MP_MODULE_MAX);
    if (m_removeModuleBtn)
        m_removeModuleBtn->setEnabled(m_modules.size() > 1);
}

void MainWindow::onAddModule()
{
    if (m_modules.size() >= MP_MODULE_MAX)
        return;
    m_modules.push_back(std::make_shared<PlayerModule>(m_modules.size()));
    rebuildModuleWidgets();
    onSelectModule(m_modules.size() - 1);
    markProjectDirty();
}

void MainWindow::onRemoveModule()
{
    if (m_modules.size() <= 1)
        return;
    int idx = m_selectedModule;
    if (idx < 0 || idx >= m_modules.size())
        idx = m_modules.size() - 1;
    m_modules.removeAt(idx);
    for (int i = 0; i < m_modules.size(); ++i) {
        if (m_modules[i])
            m_modules[i]->setIndex(i);
    }
    rebuildModuleWidgets();
    onSelectModule(qMin(idx, m_modules.size() - 1));
    markProjectDirty();
}

void MainWindow::wireModules()
{
    for (int i = 0; i < m_moduleWidgets.size(); ++i) {
        m_moduleWidgets[i]->setModule(m_modules[i]);
        connect(m_moduleWidgets[i], &PlayerModuleWidget::selected, this, &MainWindow::onSelectModule);
        connect(m_moduleWidgets[i], &PlayerModuleWidget::projectEdited, this, [this, i]() {
            markProjectDirty();
            if (m_programCanvas)
                m_programCanvas->update();
            if (i == m_selectedModule && m_clipPanel)
                m_clipPanel->reloadFromModule();
        });
        connect(m_moduleWidgets[i], &PlayerModuleWidget::openFileRequested, this,
                [this](int index) {
                    const QString path = QFileDialog::getOpenFileName(
                        this, QStringLiteral("Charger média"), m_bank->folder(),
                        QStringLiteral("Vidéo (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mpg *.mpeg "
                                       "*.wmv);;Images (*.png *.jpg *.jpeg);;Tous (*.*)"));
                    if (path.isEmpty())
                        return;
                    loadMediaIntoModule(index, path);
                });
        connect(m_moduleWidgets[i], &PlayerModuleWidget::mediaDropped, this,
                [this](int index, const QString &path) { loadMediaIntoModule(index, path); });
        connect(m_moduleWidgets[i], &PlayerModuleWidget::pausePressed, this,
                [this, i]() { setModulePauseBlink(i, true); });
        connect(m_moduleWidgets[i], &PlayerModuleWidget::playPressed, this,
                [this, i]() { setModulePauseBlink(i, false); });
        connect(m_moduleWidgets[i], &PlayerModuleWidget::stopPressed, this,
                [this, i]() { setModulePauseBlink(i, false); });
        connect(m_modules[i].get(), &PlayerModule::frameUpdated, this, [this, i]() {
            if (i < 0 || i >= m_modules.size())
                return;
            auto &mod = m_modules[i];
            if (!mod || mod->currentFrame().isNull())
                return;
            if (mod->property("sliceAlignPending").toBool()) {
                applyModuleToSlice(i, mod->linkedSliceName(), mod->sliceAlignMode(), false);
                mod->setProperty("sliceAlignPending", false);
                if (i == m_selectedModule)
                    m_clipPanel->reloadFromModule();
                markProjectDirty();
            }
        });
    }
    refreshSliceLists();
}

void MainWindow::loadMediaIntoModule(int index, const QString &path)
{
    if (index < 0 || index >= m_modules.size() || !m_modules[index] || path.isEmpty())
        return;
    if (!MediaBank::isSupportedMediaFile(path))
        return;
    onSelectModule(index);
    m_modules[index]->load(path);
    // Align to linked slice once the first frame arrives.
    if (!m_modules[index]->linkedSliceName().isEmpty())
        m_modules[index]->setProperty("sliceAlignPending", true);
    markProjectDirty();
}

void MainWindow::onSelectModule(int index)
{
    if (index >= m_modules.size())
        return;
    m_selectedModule = index;
    for (int j = 0; j < m_moduleWidgets.size(); ++j)
        m_moduleWidgets[j]->setSelected(j == index);
    m_programCanvas->setSelectedIndex(index);
    if (index >= 0 && index < m_modules.size())
        m_clipPanel->setModule(m_modules[index]);
    else
        m_clipPanel->setModule(nullptr);
}

void MainWindow::onTransformEdited()
{
    m_programCanvas->update();
    markProjectDirty();
    if (m_selectedModule >= 0 && m_selectedModule < m_moduleWidgets.size())
        m_moduleWidgets[m_selectedModule]->syncOutputVisible();
}

void MainWindow::refreshSliceLists()
{
    QStringList names;
    if (m_outputMap) {
        for (const OutputSlice &s : m_outputMap->slices())
            names.append(s.name);
    }
    if (m_clipPanel)
        m_clipPanel->setAvailableSlices(names);
}

bool MainWindow::applyModuleToSlice(int moduleIndex, const QString &sliceName,
                                    SliceAlignMode mode, bool warnIfNoMedia)
{
    if (moduleIndex < 0 || moduleIndex >= m_modules.size() || !m_modules[moduleIndex])
        return false;

    auto &mod = m_modules[moduleIndex];
    mod->setLinkedSliceName(sliceName);
    mod->setSliceAlignMode(mode);

    if (sliceName.isEmpty())
        return true;

    if (!m_outputMap) {
        if (warnIfNoMedia) {
            QMessageBox::warning(
                this, QStringLiteral("Slice"),
                QStringLiteral("Importez d'abord un Advanced Output Resolume (Output Map)."));
        }
        return false;
    }

    int sliceIdx = -1;
    for (int i = 0; i < m_outputMap->slices().size(); ++i) {
        if (m_outputMap->slices()[i].name == sliceName) {
            sliceIdx = i;
            break;
        }
    }
    if (sliceIdx < 0)
        return false;

    const QImage frame = mod->currentFrame();
    if (frame.isNull()) {
        if (warnIfNoMedia) {
            QMessageBox::warning(this, QStringLiteral("Slice"),
                                 QStringLiteral("Chargez un média dans ce module avant d'aligner."));
        }
        return false;
    }

    const OutputSlice &slice = m_outputMap->slices().at(sliceIdx);
    if (slice.inputRect.isEmpty())
        return false;

    OutputCanvas::alignTransformToRect(mod->transform(), frame, m_canvas->size(), slice.inputRect,
                                       mode);
    return true;
}

void MainWindow::onSliceLinkChanged(const QString &sliceName, SliceAlignMode mode)
{
    if (m_selectedModule < 0)
        return;
    applyModuleToSlice(m_selectedModule, sliceName, mode, !sliceName.isEmpty());
    m_clipPanel->reloadFromModule();
    if (m_programCanvas)
        m_programCanvas->update();
    markProjectDirty();
}

void MainWindow::onOutputToggled(bool onAir)
{
    if (onAir) {
        m_output->setFps(compositionFps());
        m_output->start();
    } else {
        m_output->stop();
    }
    if (m_programCanvas)
        m_programCanvas->setInteractionLocked(onAir);
    updateShowStatus();
}

void MainWindow::onProgramZoomSlider(int percent)
{
    if (m_programCanvas)
        m_programCanvas->setViewZoomPercent(percent);
    if (m_zoomPercentLabel)
        m_zoomPercentLabel->setText(QStringLiteral("%1%").arg(percent));
}

void MainWindow::onProgramZoomFit()
{
    if (m_programCanvas)
        m_programCanvas->applyZoomPreset(ProgramCanvasWidget::ZoomPreset::Fit);
}

void MainWindow::onProgramZoomOneToOne()
{
    if (m_programCanvas)
        m_programCanvas->applyZoomPreset(ProgramCanvasWidget::ZoomPreset::OneToOne);
}

void MainWindow::onProgramZoomActual()
{
    if (m_programCanvas)
        m_programCanvas->applyZoomPreset(ProgramCanvasWidget::ZoomPreset::ActualSize);
}

void MainWindow::onOpenBankFolder()
{
    const QString dir =
        QFileDialog::getExistingDirectory(this, QStringLiteral("Banque médias"), m_bank->folder());
    if (!dir.isEmpty()) {
        m_bank->setFolder(dir);
        markProjectDirty();
    }
}

void MainWindow::onBankItemActivated(const QModelIndex &index)
{
    if (!index.isValid() || m_selectedModule < 0 || m_selectedModule >= m_modules.size())
        return;
    const QStringList files = m_bank->files();
    if (index.row() < 0 || index.row() >= files.size())
        return;
    loadMediaIntoModule(m_selectedModule, files.at(index.row()));
}

void MainWindow::applyComposition(QSize size, int fps)
{
    if (size.width() < 16 || size.height() < 16)
        return;

    const QSize old = m_lastCanvasSize;
    m_canvas->setSize(size.width(), size.height());
    m_output->setResolution(size.width(), size.height());
    m_outputMap->matchCanvas(size.width(), size.height());
    m_programCanvas->setCanvasSize(size.width(), size.height());
    m_output->setFps(fps);
    m_frameTimer->setInterval(qMax(1, 1000 / qMax(1, fps)));

    if (old.width() > 0 && old.height() > 0 && old != size) {
        const float sx = static_cast<float>(size.width()) / static_cast<float>(old.width());
        const float sy = static_cast<float>(size.height()) / static_cast<float>(old.height());
        for (auto &mod : m_modules) {
            mod->transform().posX *= sx;
            mod->transform().posY *= sy;
        }
        m_clipPanel->reloadFromModule();
    }
    m_lastCanvasSize = size;
    markProjectDirty();
    updateShowStatus();
}

int MainWindow::compositionFps() const
{
    return m_output ? qMax(1, m_output->fps()) : 30;
}

void MainWindow::updateShowStatus()
{
    const bool oscOn = m_osc && m_osc->isRunning();
    const bool midiOn = m_midi && m_midi->isOpen();
    const bool ndi = m_output && m_output->ndiEnabled();
    const bool spout = m_output && m_output->spoutEnabled();
    const bool onAir = m_outputButton && m_outputButton->isOnAir();

    const auto dot = [](bool on) {
        return on ? QStringLiteral("<span style='color:#3dcf6a;'>●</span>")
                  : QStringLiteral("<span style='color:#4a5564;'>○</span>");
    };

    const QString text =
        QStringLiteral("OSC %1&nbsp;&nbsp;MIDI %2&nbsp;&nbsp;NDI %3&nbsp;&nbsp;Spout %4&nbsp;&nbsp;"
                       "<span style='color:%5;'>%6</span>")
            .arg(dot(oscOn), dot(midiOn), dot(ndi), dot(spout),
                 onAir ? QStringLiteral("#3dcf6a") : QStringLiteral("#8b95a8"),
                 onAir ? QStringLiteral("ON AIR") : QStringLiteral("STANDBY"));
    if (m_statsBar)
        m_statsBar->setShowStatus(text);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool noOrKeypad = event->modifiers() == Qt::NoModifier
                            || event->modifiers() == Qt::KeypadModifier
                            || shift;

    if (noOrKeypad) {
        if (event->key() == Qt::Key_Space) {
            if (shift) {
                onToggleAll();
            } else if (m_selectedModule >= 0 && m_selectedModule < m_modules.size()
                       && m_modules[m_selectedModule]) {
                const bool wasPlaying = m_modules[m_selectedModule]->decoder()->isPlaying();
                m_modules[m_selectedModule]->togglePlayPause();
                setModulePauseBlink(m_selectedModule, wasPlaying);
            }
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_S) {
            if (shift)
                onStopAll();
            else if (m_selectedModule >= 0 && m_selectedModule < m_modules.size()) {
                m_modules[m_selectedModule]->stop();
                setModulePauseBlink(m_selectedModule, false);
            }
            event->accept();
            return;
        }
        if (!shift && event->key() == Qt::Key_O) {
            const bool on = !(m_outputButton && m_outputButton->isOnAir());
            if (m_outputButton)
                m_outputButton->setOnAir(on);
            onOutputToggled(on);
            updateShowStatus();
            event->accept();
            return;
        }
        if (!shift && event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
            const int index = event->key() - Qt::Key_1;
            if (index < m_modules.size())
                onSelectModule(index);
            event->accept();
            return;
        }
        if (!shift && event->key() == Qt::Key_0) {
            if (m_modules.size() > 9)
                onSelectModule(9);
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::anyModulePlaying() const
{
    for (const auto &mod : m_modules) {
        if (mod && mod->decoder()->isOpen() && mod->decoder()->isPlaying())
            return true;
    }
    return false;
}

void MainWindow::updateGlobalTransportUi()
{
    const bool playing = anyModulePlaying();
    if (m_globalPlayBtn) {
        m_globalPlayBtn->setEnabled(true);
        m_globalPlayBtn->setStyleSheet(playing ? QStringLiteral(
                                                    "QPushButton { font-weight: 800; min-width: 72px;"
                                                    " background: #1e3a28; border: 1px solid #3d7a4a; }")
                                              : QStringLiteral(
                                                    "QPushButton { font-weight: 800; min-width: 72px; }"));
    }
    applyPauseBlinkStyles();
}

void MainWindow::setModulePauseBlink(int index, bool paused)
{
    if (index < 0)
        return;
    if (paused)
        m_pauseBlinkModules.insert(index);
    else
        m_pauseBlinkModules.remove(index);
    if (m_pauseBlinkModules.isEmpty())
        m_globalPauseBlink = false;
    syncPauseBlinkTimer();
    applyPauseBlinkStyles();
}

void MainWindow::setGlobalPauseBlink(bool active)
{
    m_globalPauseBlink = active;
    m_pauseBlinkModules.clear();
    if (active) {
        for (int i = 0; i < m_modules.size(); ++i) {
            const auto &mod = m_modules[i];
            if (mod && mod->decoder()->isOpen() && !mod->decoder()->isPlaying())
                m_pauseBlinkModules.insert(i);
        }
    }
    syncPauseBlinkTimer();
    applyPauseBlinkStyles();
}

void MainWindow::syncPauseBlinkTimer()
{
    if (!m_pauseBlinkTimer) {
        m_pauseBlinkTimer = new QTimer(this);
        m_pauseBlinkTimer->setInterval(450);
        connect(m_pauseBlinkTimer, &QTimer::timeout, this, &MainWindow::onPauseBlinkTick);
    }
    if (!m_pauseBlinkModules.isEmpty() || m_globalPauseBlink) {
        if (!m_pauseBlinkTimer->isActive()) {
            m_pauseBlinkPhase = true;
            m_pauseBlinkTimer->start();
        }
    } else {
        m_pauseBlinkTimer->stop();
        m_pauseBlinkPhase = false;
    }
}

void MainWindow::onPauseBlinkTick()
{
    m_pauseBlinkPhase = !m_pauseBlinkPhase;
    applyPauseBlinkStyles();
}

void MainWindow::applyPauseBlinkStyles()
{
    for (int i = 0; i < m_moduleWidgets.size(); ++i) {
        auto *w = m_moduleWidgets[i];
        if (!w)
            continue;
        w->setPauseBlink(m_pauseBlinkModules.contains(i), m_pauseBlinkPhase);
    }
    if (!m_globalPauseBtn)
        return;
    if (m_globalPauseBlink && !m_pauseBlinkModules.isEmpty()) {
        m_globalPauseBtn->setStyleSheet(
            m_pauseBlinkPhase
                ? QStringLiteral(
                      "QPushButton { font-weight: 800; min-width: 72px;"
                      " background: #e07020; border: 1px solid #ffb060; color: #1a1008; }")
                : QStringLiteral(
                      "QPushButton { font-weight: 800; min-width: 72px;"
                      " background: #4a2a10; border: 1px solid #a06030; color: #ffc080; }"));
    } else {
        m_globalPauseBtn->setStyleSheet(QStringLiteral("QPushButton { font-weight: 800; min-width: 72px; }"));
    }
}

void MainWindow::onPlayAll()
{
    for (auto &mod : m_modules) {
        if (mod && mod->decoder()->isOpen())
            mod->play();
    }
    setGlobalPauseBlink(false);
    updateGlobalTransportUi();
}

void MainWindow::onPauseAll()
{
    for (auto &mod : m_modules) {
        if (mod)
            mod->pause();
    }
    setGlobalPauseBlink(true);
    updateGlobalTransportUi();
}

void MainWindow::onStopAll()
{
    for (auto &mod : m_modules) {
        if (mod)
            mod->stop();
    }
    setGlobalPauseBlink(false);
    updateGlobalTransportUi();
}

void MainWindow::onToggleAll()
{
    if (anyModulePlaying())
        onPauseAll();
    else
        onPlayAll();
}

void MainWindow::composeAndSend()
{
    QImage composed = m_canvas->compose(m_modules, m_outputMap.get());
    applyAlphaMaskToImage(composed);

    // Air out (NDI/Spout) may go through Output Map. Program keeps the composition
    // so the input guide stays visible behind transparent pixels.
    QImage airFrame = composed;
    if (m_outputMap && !m_outputMap->isPassthrough(composed.size())) {
        const QImage mapped = m_outputMap->apply(composed);
        if (!mapped.isNull())
            airFrame = mapped;
    }

    m_programCanvas->setFrame(composed);

    if (m_outputButton->isOnAir())
        m_output->sendFrame(airFrame);
}

void MainWindow::onFrameTick()
{
    composeAndSend();
}

void MainWindow::onPerfTick()
{
    m_perf->update();
    const double fps = compositionFps();
    m_statsBar->setStats(m_perf->cpuPercent(), m_perf->gpuPercent(), m_perf->ramUsedMb(),
                         m_perf->ramTotalMb(), fps);
    updateShowStatus();
    updateGlobalTransportUi();
}

void MainWindow::onLoadProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Ouvrir projet"), defaultProjectPath(),
        QStringLiteral("Projet MultiPlayer (*.mpproj);;JSON (*.json)"));
    if (path.isEmpty())
        return;
    loadProjectFromPath(path);
}

void MainWindow::onOpenRecentProject()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    const QString path = action->data().toString();
    if (path.isEmpty())
        return;
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Projet"),
                             QStringLiteral("Fichier introuvable :\n%1").arg(path));
        rememberRecentProject(path); // will drop missing on rebuild via filter
        // Remove missing entry
        QStringList recent = recentProjects();
        recent.removeAll(path);
        QSettings().setValue(QStringLiteral("recentProjects"), recent);
        rebuildRecentMenu();
        return;
    }
    loadProjectFromPath(path);
}

bool MainWindow::loadProjectFromPath(const QString &path)
{
    int fps = 30;
    InputGuideState guide;
    AlphaMaskState alphaMask;
    if (!ProjectFile::load(path, m_modules, m_bank.get(), m_canvas.get(), m_output.get(),
                           m_outputMap.get(), &fps, &guide, &alphaMask)) {
        QMessageBox::warning(this, QStringLiteral("Projet"),
                             QStringLiteral("Impossible de charger le projet."));
        return false;
    }
    m_currentProjectPath = path;
    m_output->setFps(fps);
    m_frameTimer->setInterval(qMax(1, 1000 / qMax(1, fps)));
    m_lastCanvasSize = m_canvas->size();
    m_outputMap->matchCanvas(m_lastCanvasSize.width(), m_lastCanvasSize.height());
    m_output->setResolution(m_lastCanvasSize.width(), m_lastCanvasSize.height());
    m_programCanvas->setCanvasSize(m_lastCanvasSize.width(), m_lastCanvasSize.height());
    m_inputGuide = guide;
    m_guideImage = QImage();
    applyInputGuideToCanvas();
    m_alphaMask = alphaMask;
    m_alphaMaskImage = QImage();
    invalidateAlphaMaskCache();
    applyAlphaMaskUi();
    rebuildModuleWidgets();
    onSelectModule(-1);
    rememberRecentProject(path);
    QSettings().setValue(QStringLiteral("lastProject"), path);
    refreshSliceLists();
    clearProjectDirty();
    updateShowStatus();
    updateWindowTitle();
    return true;
}

QStringList MainWindow::recentProjects() const
{
    return QSettings().value(QStringLiteral("recentProjects")).toStringList();
}

void MainWindow::rememberRecentProject(const QString &path)
{
    if (path.isEmpty())
        return;
    QStringList recent = recentProjects();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    QSettings().setValue(QStringLiteral("recentProjects"), recent);
    QSettings().setValue(QStringLiteral("lastProject"), path);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    const QStringList recent = recentProjects();
    int added = 0;
    for (const QString &path : recent) {
        if (!QFileInfo::exists(path))
            continue;
        auto *action = m_recentMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        action->setData(path);
        connect(action, &QAction::triggered, this, &MainWindow::onOpenRecentProject);
        ++added;
    }
    if (added == 0) {
        auto *empty = m_recentMenu->addAction(QStringLiteral("(aucun)"));
        empty->setEnabled(false);
    } else {
        m_recentMenu->addSeparator();
        auto *clear = m_recentMenu->addAction(QStringLiteral("Effacer la liste"));
        connect(clear, &QAction::triggered, this, [this]() {
            QSettings().remove(QStringLiteral("recentProjects"));
            rebuildRecentMenu();
        });
    }
}

void MainWindow::onSaveProject()
{
    trySaveProject();
}

void MainWindow::onSaveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Enregistrer projet"), defaultProjectPath(),
        QStringLiteral("Projet MultiPlayer (*.mpproj)"));
    if (path.isEmpty())
        return;
    m_currentProjectPath = path.endsWith(QStringLiteral(".mpproj"))
                               ? path
                               : path + QStringLiteral(".mpproj");
    trySaveProject();
}

bool MainWindow::trySaveProject()
{
    if (m_currentProjectPath.isEmpty()) {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Enregistrer projet"), defaultProjectPath(),
            QStringLiteral("Projet MultiPlayer (*.mpproj)"));
        if (path.isEmpty())
            return false;
        m_currentProjectPath = path.endsWith(QStringLiteral(".mpproj"))
                                   ? path
                                   : path + QStringLiteral(".mpproj");
    }
    const InputGuideState guide = currentGuideState();
    const AlphaMaskState alphaMask = currentAlphaMaskState();
    if (!ProjectFile::save(m_currentProjectPath, m_modules, m_bank.get(), m_canvas.get(),
                           m_output.get(), m_outputMap.get(), compositionFps(), &guide,
                           &alphaMask)) {
        QMessageBox::warning(this, QStringLiteral("Projet"),
                             QStringLiteral("Échec de l'enregistrement."));
        return false;
    }
    QSettings().setValue(QStringLiteral("lastProject"), m_currentProjectPath);
    rememberRecentProject(m_currentProjectPath);
    clearProjectDirty();
    return true;
}

QString MainWindow::defaultProjectPath() const
{
    return QDir::homePath() + QStringLiteral("/MultiPlayer");
}

void MainWindow::onOpenOutputMap()
{
    OutputMapDialog dlg(m_outputMap.get(), &m_modules, m_canvas->size().width(),
                        m_canvas->size().height(), this);
    connect(&dlg, &OutputMapDialog::moduleLinksChanged, this, [this]() {
        m_clipPanel->reloadFromModule();
        if (m_programCanvas)
            m_programCanvas->update();
        markProjectDirty();
    });
    dlg.exec();
    refreshSliceLists();
    m_clipPanel->reloadFromModule();
    if (m_programCanvas)
        m_programCanvas->update();
    markProjectDirty();
}

void MainWindow::setupAutosave()
{
    if (!m_autosaveTimer) {
        m_autosaveTimer = new QTimer(this);
        m_autosaveTimer->setInterval(60 * 1000);
        connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::onAutosaveTick);
    }
    if (QSettings().value(QStringLiteral("project/autosave"), false).toBool())
        m_autosaveTimer->start();
    else
        m_autosaveTimer->stop();
}

void MainWindow::onAutosaveTick()
{
    if (!QSettings().value(QStringLiteral("project/autosave"), false).toBool())
        return;
    if (!m_projectDirty || m_currentProjectPath.isEmpty())
        return;
    const InputGuideState guide = currentGuideState();
    const AlphaMaskState alphaMask = currentAlphaMaskState();
    if (ProjectFile::save(m_currentProjectPath, m_modules, m_bank.get(), m_canvas.get(),
                          m_output.get(), m_outputMap.get(), compositionFps(), &guide,
                          &alphaMask)) {
        clearProjectDirty();
        statusBar()->showMessage(QStringLiteral("Autosave — %1")
                                     .arg(QFileInfo(m_currentProjectPath).fileName()),
                                 3000);
    }
}

void MainWindow::invalidateAlphaMaskCache()
{
    m_alphaMaskPrepared = QImage();
    m_alphaMaskPreparedSize = QSize();
    m_alphaMaskPreparedInverted = false;
}

void MainWindow::applyAlphaMaskToImage(QImage &frame)
{
    if (!m_alphaMask.visible || m_alphaMaskImage.isNull() || frame.isNull())
        return;

    const qreal opacity = qBound(0.0, static_cast<qreal>(m_alphaMask.opacity), 1.0);
    if (opacity <= 0.001)
        return;

    QImage out = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QSize target = out.size();

    if (m_alphaMaskPrepared.size() != target
        || m_alphaMaskPreparedInverted != m_alphaMask.inverted
        || m_alphaMaskPreparedSize != target) {
        QImage mask = m_alphaMaskImage.convertToFormat(QImage::Format_ARGB32);
        if (mask.size() != target)
            mask = mask.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        if (m_alphaMask.inverted) {
            for (int y = 0; y < mask.height(); ++y) {
                auto *line = reinterpret_cast<QRgb *>(mask.scanLine(y));
                for (int x = 0; x < mask.width(); ++x) {
                    const QRgb px = line[x];
                    const int a = qAlpha(px);
                    const int ia = 255 - a;
                    int r = qRed(px);
                    int g = qGreen(px);
                    int b = qBlue(px);
                    if (a == 0) {
                        r = 255;
                        g = 255;
                        b = 255;
                    }
                    line[x] = qRgba(r, g, b, ia);
                }
            }
        }

        m_alphaMaskPrepared = mask.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        m_alphaMaskPreparedSize = target;
        m_alphaMaskPreparedInverted = m_alphaMask.inverted;
    }

    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setOpacity(opacity);
    p.drawImage(0, 0, m_alphaMaskPrepared);
    p.end();
    frame = out.convertToFormat(QImage::Format_ARGB32);
}

void MainWindow::applyAlphaMaskUi()
{
    if (m_alphaMaskOpacity) {
        const QSignalBlocker b(m_alphaMaskOpacity);
        m_alphaMaskOpacity->setValue(
            qBound(0, static_cast<int>(m_alphaMask.opacity * 100.f + 0.5f), 100));
    }

    if (m_alphaMaskVisible) {
        const QSignalBlocker b(m_alphaMaskVisible);
        m_alphaMaskVisible->setChecked(m_alphaMask.visible);
    }
    if (m_alphaMaskInvert) {
        const QSignalBlocker b(m_alphaMaskInvert);
        m_alphaMaskInvert->setChecked(m_alphaMask.inverted);
    }

    if (m_alphaMask.path.isEmpty()) {
        m_alphaMaskImage = QImage();
        invalidateAlphaMaskCache();
        if (m_alphaMaskPathLabel) {
            m_alphaMaskPathLabel->setText(QStringLiteral("—"));
            m_alphaMaskPathLabel->setToolTip(QString());
        }
        return;
    }

    if (m_alphaMaskImage.isNull())
        m_alphaMaskImage = QImage(m_alphaMask.path);

    if (m_alphaMaskPathLabel) {
        if (m_alphaMaskImage.isNull()) {
            m_alphaMaskPathLabel->setText(QStringLiteral("(fichier introuvable)"));
            m_alphaMaskPathLabel->setToolTip(m_alphaMask.path);
        } else {
            m_alphaMaskPathLabel->setText(QFileInfo(m_alphaMask.path).fileName());
            m_alphaMaskPathLabel->setToolTip(m_alphaMask.path);
        }
    }
}

void MainWindow::applyInputGuideToCanvas()
{
    if (!m_programCanvas)
        return;

    if (m_inputGuide.path.isEmpty()) {
        m_guideImage = QImage();
        m_programCanvas->clearInputGuide();
        if (m_guidePathLabel) {
            m_guidePathLabel->setText(QStringLiteral("—"));
            m_guidePathLabel->setToolTip(QString());
        }
        return;
    }

    if (m_guideImage.isNull())
        m_guideImage = QImage(m_inputGuide.path);

    if (m_guideOpacity) {
        const QSignalBlocker b(m_guideOpacity);
        m_guideOpacity->setValue(
            qBound(0, static_cast<int>(m_inputGuide.opacity * 100.f + 0.5f), 100));
    }
    if (m_guideVisible) {
        const QSignalBlocker b(m_guideVisible);
        m_guideVisible->setChecked(m_inputGuide.visible);
    }
    if (m_guideForeground) {
        const QSignalBlocker b(m_guideForeground);
        m_guideForeground->setChecked(m_inputGuide.foreground);
    }
    if (m_guidePathLabel) {
        m_guidePathLabel->setText(QFileInfo(m_inputGuide.path).fileName());
        m_guidePathLabel->setToolTip(m_inputGuide.path);
    }

    if (m_guideImage.isNull()) {
        m_programCanvas->clearInputGuide();
        if (m_guidePathLabel)
            m_guidePathLabel->setText(QStringLiteral("(fichier introuvable)"));
        return;
    }

    m_programCanvas->setInputGuide(m_guideImage, m_inputGuide.opacity, m_inputGuide.visible,
                                   m_inputGuide.foreground);
}

void MainWindow::onLoadInputGuide()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Input guide (photo)"), m_bank->folder(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp);;Tous (*.*)"));
    if (path.isEmpty())
        return;
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Input guide"),
                             QStringLiteral("Impossible de charger l'image."));
        return;
    }
    m_inputGuide.path = path;
    m_inputGuide.opacity = m_guideOpacity ? m_guideOpacity->value() / 100.f : 0.45f;
    m_inputGuide.visible = !m_guideVisible || m_guideVisible->isChecked();
    m_inputGuide.foreground = m_guideForeground && m_guideForeground->isChecked();
    m_guideImage = img;
    applyInputGuideToCanvas();
    markProjectDirty();
}

void MainWindow::onClearInputGuide()
{
    m_inputGuide = InputGuideState{};
    m_guideImage = QImage();
    applyInputGuideToCanvas();
    markProjectDirty();
}

void MainWindow::onGuideOpacityChanged(int value)
{
    m_inputGuide.opacity = value / 100.f;
    applyInputGuideToCanvas();
    markProjectDirty();
}

void MainWindow::onGuideVisibleToggled(bool visible)
{
    m_inputGuide.visible = visible;
    applyInputGuideToCanvas();
    markProjectDirty();
}

void MainWindow::onGuideForegroundToggled(bool foreground)
{
    m_inputGuide.foreground = foreground;
    applyInputGuideToCanvas();
    markProjectDirty();
}

void MainWindow::onLoadAlphaMask()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Masque alpha (PNG)"), m_bank->folder(),
        QStringLiteral("PNG avec transparence (*.png);;Images (*.png *.webp);;Tous (*.*)"));
    if (path.isEmpty())
        return;
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Masque alpha"),
                             QStringLiteral("Impossible de charger l'image."));
        return;
    }
    if (img.format() != QImage::Format_ARGB32
        && img.format() != QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32);

    m_alphaMask.path = path;
    m_alphaMask.opacity = m_alphaMaskOpacity ? m_alphaMaskOpacity->value() / 100.f : 1.f;
    m_alphaMask.visible = !m_alphaMaskVisible || m_alphaMaskVisible->isChecked();
    m_alphaMask.inverted = m_alphaMaskInvert && m_alphaMaskInvert->isChecked();
    m_alphaMaskImage = img;
    invalidateAlphaMaskCache();
    applyAlphaMaskUi();
    markProjectDirty();
}

void MainWindow::onClearAlphaMask()
{
    m_alphaMask = AlphaMaskState{};
    m_alphaMaskImage = QImage();
    invalidateAlphaMaskCache();
    applyAlphaMaskUi();
    markProjectDirty();
}

void MainWindow::onAlphaMaskVisibleToggled(bool visible)
{
    m_alphaMask.visible = visible;
    markProjectDirty();
}

void MainWindow::onAlphaMaskInvertedToggled(bool inverted)
{
    m_alphaMask.inverted = inverted;
    invalidateAlphaMaskCache();
    markProjectDirty();
}

void MainWindow::onAlphaMaskOpacityChanged(int value)
{
    m_alphaMask.opacity = value / 100.f;
    markProjectDirty();
}

void MainWindow::markProjectDirty()
{
    if (m_projectDirty)
        return;
    m_projectDirty = true;
    updateWindowTitle();
}

void MainWindow::clearProjectDirty()
{
    m_projectDirty = false;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("%1 — %2").arg(QStringLiteral(MP_APP_NAME),
                                                  QStringLiteral(MP_APP_ID));
    if (!m_currentProjectPath.isEmpty())
        title += QStringLiteral(" — %1").arg(QFileInfo(m_currentProjectPath).fileName());
    if (m_projectDirty)
        title += QStringLiteral(" *");
    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_projectDirty) {
        event->accept();
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Projet non enregistré"));
    box.setText(QStringLiteral("Enregistrer les modifications avant de quitter ?"));
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Save);
    if (auto *saveBtn = box.button(QMessageBox::Save))
        saveBtn->setText(QStringLiteral("Enregistrer"));
    if (auto *discardBtn = box.button(QMessageBox::Discard))
        discardBtn->setText(QStringLiteral("Ne pas enregistrer"));
    if (auto *cancelBtn = box.button(QMessageBox::Cancel))
        cancelBtn->setText(QStringLiteral("Annuler"));

    const int result = box.exec();
    if (result == QMessageBox::Cancel) {
        event->ignore();
        return;
    }
    if (result == QMessageBox::Save) {
        if (!trySaveProject()) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::setupControl()
{
    m_router->setModules(&m_modules);
    m_router->setOutputToggle([this](bool on) {
        if (m_outputButton->isOnAir() != on)
            m_outputButton->setOnAir(on);
        onOutputToggled(on);
    });
    m_router->setOutputQuery([this]() { return m_outputButton->isOnAir(); });
    m_router->setSelectModule([this](int i) { onSelectModule(i); });
    m_router->setTransformNotify([this]() {
        m_clipPanel->reloadFromModule();
        m_programCanvas->update();
        for (auto *w : m_moduleWidgets)
            w->syncOutputVisible();
    });
    m_router->setPlayAll([this]() { onPlayAll(); });
    m_router->setPauseAll([this]() { onPauseAll(); });
    m_router->setStopAll([this]() { onStopAll(); });
    m_router->setToggleAll([this]() { onToggleAll(); });

    connect(m_osc.get(), &OscServer::messageReceived, m_router.get(), &ControlRouter::handleOsc);
    connect(m_midi.get(), &MidiController::actionTriggered, m_router.get(),
            &ControlRouter::handleMidiAction);
    connect(m_midi.get(), &MidiController::learned, this, [this](const MidiBinding &) {
        saveMidiBindings();
    });

    loadMidiBindings();

    QSettings s;
    const bool oscEnabled = s.value(QStringLiteral("osc/enabled"), true).toBool();
    const bool midiEnabled = s.value(QStringLiteral("midi/enabled"), true).toBool();
    const int port = s.value(QStringLiteral("osc/port"), 7000).toInt();

    if (oscEnabled)
        m_osc->start(static_cast<quint16>(port));
    else
        m_osc->stop();

    if (midiEnabled) {
        const int midiPort = s.value(QStringLiteral("midi/port"), -1).toInt();
        if (midiPort >= 0 && midiPort < m_midi->availablePorts().size())
            m_midi->openPort(midiPort);
    } else {
        m_midi->closePort();
    }
    updateShowStatus();
}

void MainWindow::saveMidiBindings() const
{
    QSettings s;
    const auto bindings = m_midi->bindings();
    s.beginWriteArray(QStringLiteral("midi/bindings"), bindings.size());
    for (int i = 0; i < bindings.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("type"),
                   bindings[i].type == MidiBinding::Type::NoteOn ? 0 : 1);
        s.setValue(QStringLiteral("channel"), bindings[i].channel);
        s.setValue(QStringLiteral("number"), bindings[i].number);
        s.setValue(QStringLiteral("action"), bindings[i].action);
    }
    s.endArray();
}

void MainWindow::loadMidiBindings()
{
    QSettings s;
    const int n = s.beginReadArray(QStringLiteral("midi/bindings"));
    QVector<MidiBinding> bindings;
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        MidiBinding b;
        b.type = s.value(QStringLiteral("type")).toInt() == 0 ? MidiBinding::Type::NoteOn
                                                              : MidiBinding::Type::ControlChange;
        b.channel = s.value(QStringLiteral("channel")).toInt();
        b.number = s.value(QStringLiteral("number")).toInt();
        b.action = s.value(QStringLiteral("action")).toString();
        if (!b.action.isEmpty())
            bindings.push_back(b);
    }
    s.endArray();
    m_midi->setBindings(bindings);
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(m_osc.get(), m_midi.get(), m_output.get(), m_lastCanvasSize,
                       compositionFps(), this);
    connect(&dlg, &SettingsDialog::applyComposition, this, &MainWindow::applyComposition);
    connect(&dlg, &SettingsDialog::autosaveChanged, this, [this](bool) { setupAutosave(); });
    connect(&dlg, &SettingsDialog::outputsChanged, this, [this]() {
        markProjectDirty();
        updateShowStatus();
    });
    dlg.exec();
    setupAutosave();
    saveMidiBindings();
    updateShowStatus();
}
