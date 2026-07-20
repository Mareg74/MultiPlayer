#pragma once

#include "compose/OutputCanvas.h"
#include "map/OutputMap.h"
#include "media/PlayerModule.h"

#include <QDialog>
#include <QVector>
#include <memory>

class QTableWidget;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class SliceWarpEditor;
class QLabel;
class QTabWidget;

class OutputMapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OutputMapDialog(OutputMap *map,
                             QVector<std::shared_ptr<PlayerModule>> *modules,
                             int canvasW,
                             int canvasH,
                             QWidget *parent = nullptr);

signals:
    void moduleLinksChanged();

private slots:
    void onAdd();
    void onRemove();
    void onReset();
    void onImportResolume();
    void onExportJson();
    void onImportJson();
    void syncFromUi();
    void reloadTable();
    void reloadLinkTable();
    void onSelectionChanged();
    void onSoftChanged();
    void onWarpToggled(bool on);
    void onResetCorners();
    void onLinkCellChanged(int row, int column);

private:
    void applyModuleSliceLink(int moduleIndex, const QString &sliceName, SliceAlignMode mode);
    QStringList sliceNames() const;

    OutputMap *m_map = nullptr;
    QVector<std::shared_ptr<PlayerModule>> *m_modules = nullptr;
    int m_canvasW = 1920;
    int m_canvasH = 1080;
    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_table = nullptr;
    QTableWidget *m_linkTable = nullptr;
    QSpinBox *m_outW = nullptr;
    QSpinBox *m_outH = nullptr;
    SliceWarpEditor *m_warpEditor = nullptr;
    QCheckBox *m_useWarp = nullptr;
    QDoubleSpinBox *m_softL = nullptr;
    QDoubleSpinBox *m_softR = nullptr;
    QDoubleSpinBox *m_softT = nullptr;
    QDoubleSpinBox *m_softB = nullptr;
    QLabel *m_sliceLabel = nullptr;
    bool m_updating = false;
};
