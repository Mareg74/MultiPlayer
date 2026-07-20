#include "ui/OutputMapDialog.h"
#include "map/ResolumeXmlImport.h"
#include "ui/SliceWarpEditor.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

OutputMapDialog::OutputMapDialog(OutputMap *map,
                                 QVector<std::shared_ptr<PlayerModule>> *modules,
                                 int canvasW,
                                 int canvasH,
                                 QWidget *parent)
    : QDialog(parent)
    , m_map(map)
    , m_modules(modules)
    , m_canvasW(canvasW)
    , m_canvasH(canvasH)
{
    setWindowTitle(QStringLiteral("Output Map"));
    resize(980, 680);

    auto *root = new QVBoxLayout(this);

    auto *sizeRow = new QHBoxLayout();
    m_outW = new QSpinBox(this);
    m_outH = new QSpinBox(this);
    m_outW->setRange(16, 16384);
    m_outH->setRange(16, 16384);
    m_outW->setValue(map->outputSize().width());
    m_outH->setValue(map->outputSize().height());
    sizeRow->addWidget(new QLabel(QStringLiteral("Output W"), this));
    sizeRow->addWidget(m_outW);
    sizeRow->addWidget(new QLabel(QStringLiteral("H"), this));
    sizeRow->addWidget(m_outH);
    sizeRow->addStretch();
    root->addLayout(sizeRow);

    m_tabs = new QTabWidget(this);

    // —— Tab Slices (warp & soft-edge) ——
    auto *slicesPage = new QWidget(m_tabs);
    auto *split = new QHBoxLayout(slicesPage);

    m_table = new QTableWidget(slicesPage);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Nom"), QStringLiteral("In X"), QStringLiteral("In Y"),
         QStringLiteral("In W"), QStringLiteral("In H"), QStringLiteral("Out X"),
         QStringLiteral("Out Y"), QStringLiteral("Out W"), QStringLiteral("Out H")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    split->addWidget(m_table, 2);

    auto *right = new QVBoxLayout();
    m_sliceLabel = new QLabel(QStringLiteral("Slice: —"), slicesPage);
    right->addWidget(m_sliceLabel);

    m_useWarp = new QCheckBox(QStringLiteral("Warp perspective (coins)"), slicesPage);
    right->addWidget(m_useWarp);

    m_warpEditor = new SliceWarpEditor(slicesPage);
    m_warpEditor->setOutputSize(m_outW->value(), m_outH->value());
    right->addWidget(m_warpEditor, 1);

    auto *softBox = new QFormLayout();
    auto makeSoft = [this, slicesPage]() {
        auto *s = new QDoubleSpinBox(slicesPage);
        s->setRange(0.0, 0.49);
        s->setSingleStep(0.01);
        s->setDecimals(3);
        return s;
    };
    m_softL = makeSoft();
    m_softR = makeSoft();
    m_softT = makeSoft();
    m_softB = makeSoft();
    softBox->addRow(QStringLiteral("Soft L"), m_softL);
    softBox->addRow(QStringLiteral("Soft R"), m_softR);
    softBox->addRow(QStringLiteral("Soft T"), m_softT);
    softBox->addRow(QStringLiteral("Soft B"), m_softB);
    right->addLayout(softBox);

    auto *resetCorners = new QPushButton(QStringLiteral("Reset coins → rectangle"), slicesPage);
    right->addWidget(resetCorners);

    split->addLayout(right, 1);
    m_tabs->addTab(slicesPage, QStringLiteral("Slices"));

    // —— Tab Modules ↔ Slices ——
    auto *linkPage = new QWidget(m_tabs);
    auto *linkLay = new QVBoxLayout(linkPage);
    linkLay->addWidget(new QLabel(
        QStringLiteral("Associer chaque module à une slice (inputRect). Vide = aucune liaison."),
        linkPage));
    m_linkTable = new QTableWidget(linkPage);
    m_linkTable->setColumnCount(3);
    m_linkTable->setHorizontalHeaderLabels(
        {QStringLiteral("Module"), QStringLiteral("Slice"), QStringLiteral("Mode")});
    m_linkTable->horizontalHeader()->setStretchLastSection(true);
    m_linkTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_linkTable->verticalHeader()->setVisible(false);
    linkLay->addWidget(m_linkTable, 1);
    m_tabs->addTab(linkPage, QStringLiteral("Modules ↔ Slices"));

    root->addWidget(m_tabs, 1);

    auto *btns = new QHBoxLayout();
    auto *addBtn = new QPushButton(QStringLiteral("Ajouter"), this);
    auto *rmBtn = new QPushButton(QStringLiteral("Supprimer"), this);
    auto *resetBtn = new QPushButton(QStringLiteral("Fullscreen"), this);
    auto *impRslm = new QPushButton(QStringLiteral("Import Resolume (XML/RSLM)…"), this);
    auto *expJson = new QPushButton(QStringLiteral("Export JSON…"), this);
    auto *impJson = new QPushButton(QStringLiteral("Import JSON…"), this);
    auto *ok = new QPushButton(QStringLiteral("OK"), this);
    btns->addWidget(addBtn);
    btns->addWidget(rmBtn);
    btns->addWidget(resetBtn);
    btns->addWidget(impRslm);
    btns->addWidget(expJson);
    btns->addWidget(impJson);
    btns->addStretch();
    btns->addWidget(ok);
    root->addLayout(btns);

    connect(addBtn, &QPushButton::clicked, this, &OutputMapDialog::onAdd);
    connect(rmBtn, &QPushButton::clicked, this, &OutputMapDialog::onRemove);
    connect(resetBtn, &QPushButton::clicked, this, &OutputMapDialog::onReset);
    connect(impRslm, &QPushButton::clicked, this, &OutputMapDialog::onImportResolume);
    connect(expJson, &QPushButton::clicked, this, &OutputMapDialog::onExportJson);
    connect(impJson, &QPushButton::clicked, this, &OutputMapDialog::onImportJson);
    connect(ok, &QPushButton::clicked, this, [this]() {
        syncFromUi();
        accept();
    });
    connect(m_outW, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int w) {
        m_map->setOutputSize(w, m_outH->value());
        m_warpEditor->setOutputSize(w, m_outH->value());
    });
    connect(m_outH, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int h) {
        m_map->setOutputSize(m_outW->value(), h);
        m_warpEditor->setOutputSize(m_outW->value(), h);
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &OutputMapDialog::onSelectionChanged);
    connect(m_useWarp, &QCheckBox::toggled, this, &OutputMapDialog::onWarpToggled);
    connect(m_softL, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &OutputMapDialog::onSoftChanged);
    connect(m_softR, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &OutputMapDialog::onSoftChanged);
    connect(m_softT, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &OutputMapDialog::onSoftChanged);
    connect(m_softB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &OutputMapDialog::onSoftChanged);
    connect(m_warpEditor, &SliceWarpEditor::sliceChanged, this, [this]() {
        const int row = m_table->currentRow();
        if (row < 0 || row >= m_map->slices().size())
            return;
        const auto &s = m_map->slices()[row];
        m_updating = true;
        if (m_table->item(row, 5))
            m_table->item(row, 5)->setText(QString::number(s.outputRect.x()));
        if (m_table->item(row, 6))
            m_table->item(row, 6)->setText(QString::number(s.outputRect.y()));
        if (m_table->item(row, 7))
            m_table->item(row, 7)->setText(QString::number(s.outputRect.width()));
        if (m_table->item(row, 8))
            m_table->item(row, 8)->setText(QString::number(s.outputRect.height()));
        m_updating = false;
    });
    connect(resetCorners, &QPushButton::clicked, this, &OutputMapDialog::onResetCorners);
    connect(m_linkTable, &QTableWidget::cellChanged, this, &OutputMapDialog::onLinkCellChanged);

    reloadTable();
    reloadLinkTable();
    if (m_table->rowCount() > 0)
        m_table->selectRow(0);
}

QStringList OutputMapDialog::sliceNames() const
{
    QStringList names;
    if (!m_map)
        return names;
    for (const OutputSlice &s : m_map->slices())
        names.append(s.name);
    return names;
}

void OutputMapDialog::reloadTable()
{
    m_table->setRowCount(0);
    const auto &slices = m_map->slices();
    m_table->setRowCount(slices.size());
    for (int i = 0; i < slices.size(); ++i) {
        const OutputSlice &s = slices[i];
        auto fmt = [](qreal v) {
            if (qAbs(v) < 1e-6)
                return QStringLiteral("0");
            if (qAbs(v - qRound(v)) < 1e-3)
                return QString::number(qRound(v));
            return QString::number(v, 'f', 3);
        };
        m_table->setItem(i, 0, new QTableWidgetItem(s.name));
        m_table->setItem(i, 1, new QTableWidgetItem(fmt(s.inputRect.x())));
        m_table->setItem(i, 2, new QTableWidgetItem(fmt(s.inputRect.y())));
        m_table->setItem(i, 3, new QTableWidgetItem(fmt(s.inputRect.width())));
        m_table->setItem(i, 4, new QTableWidgetItem(fmt(s.inputRect.height())));
        m_table->setItem(i, 5, new QTableWidgetItem(fmt(s.outputRect.x())));
        m_table->setItem(i, 6, new QTableWidgetItem(fmt(s.outputRect.y())));
        m_table->setItem(i, 7, new QTableWidgetItem(fmt(s.outputRect.width())));
        m_table->setItem(i, 8, new QTableWidgetItem(fmt(s.outputRect.height())));
    }
    reloadLinkTable();
}

void OutputMapDialog::reloadLinkTable()
{
    if (!m_linkTable || !m_modules)
        return;

    QSignalBlocker block(m_linkTable);
    m_updating = true;
    m_linkTable->clearContents();
    m_linkTable->setRowCount(m_modules->size());
    const QStringList names = sliceNames();

    for (int i = 0; i < m_modules->size(); ++i) {
        const auto &mod = (*m_modules)[i];
        auto *modItem = new QTableWidgetItem(mod ? mod->name()
                                                 : PlayerModule::defaultNameForIndex(i));
        modItem->setFlags(modItem->flags() & ~Qt::ItemIsEditable);
        m_linkTable->setItem(i, 0, modItem);

        auto *sliceCombo = new QComboBox(m_linkTable);
        sliceCombo->addItem(QStringLiteral("—"), QString());
        for (const QString &n : names)
            sliceCombo->addItem(n, n);
        if (mod) {
            const int idx = sliceCombo->findData(mod->linkedSliceName());
            sliceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        }
        m_linkTable->setCellWidget(i, 1, sliceCombo);

        auto *modeCombo = new QComboBox(m_linkTable);
        modeCombo->addItem(QStringLiteral("Fit"), static_cast<int>(SliceAlignMode::Fit));
        modeCombo->addItem(QStringLiteral("Fill"), static_cast<int>(SliceAlignMode::Fill));
        modeCombo->addItem(QStringLiteral("Stretch"), static_cast<int>(SliceAlignMode::Stretch));
        if (mod) {
            const int midx = modeCombo->findData(static_cast<int>(mod->sliceAlignMode()));
            modeCombo->setCurrentIndex(midx >= 0 ? midx : 0);
        }
        const bool hasSlice = sliceCombo->currentData().toString().isEmpty() == false;
        modeCombo->setEnabled(hasSlice);
        m_linkTable->setCellWidget(i, 2, modeCombo);

        connect(sliceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, i, sliceCombo, modeCombo](int) {
                    if (m_updating)
                        return;
                    const QString name = sliceCombo->currentData().toString();
                    modeCombo->setEnabled(!name.isEmpty());
                    const auto mode =
                        static_cast<SliceAlignMode>(modeCombo->currentData().toInt());
                    applyModuleSliceLink(i, name, mode);
                });
        connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, i, sliceCombo, modeCombo](int) {
                    if (m_updating)
                        return;
                    const QString name = sliceCombo->currentData().toString();
                    if (name.isEmpty())
                        return;
                    const auto mode =
                        static_cast<SliceAlignMode>(modeCombo->currentData().toInt());
                    applyModuleSliceLink(i, name, mode);
                });
    }
    m_updating = false;
}

void OutputMapDialog::applyModuleSliceLink(int moduleIndex,
                                           const QString &sliceName,
                                           SliceAlignMode mode)
{
    if (!m_modules || moduleIndex < 0 || moduleIndex >= m_modules->size())
        return;
    auto &mod = (*m_modules)[moduleIndex];
    if (!mod)
        return;

    mod->setLinkedSliceName(sliceName);
    mod->setSliceAlignMode(mode);

    if (sliceName.isEmpty() || !m_map)
        return;

    int sliceIdx = -1;
    for (int i = 0; i < m_map->slices().size(); ++i) {
        if (m_map->slices()[i].name == sliceName) {
            sliceIdx = i;
            break;
        }
    }
    if (sliceIdx < 0)
        return;

    const QImage frame = mod->currentFrame();
    if (frame.isNull())
        return;

    const OutputSlice &slice = m_map->slices().at(sliceIdx);
    if (slice.inputRect.isEmpty())
        return;

    OutputCanvas::alignTransformToRect(mod->transform(), frame, QSize(m_canvasW, m_canvasH),
                                       slice.inputRect, mode);
    emit moduleLinksChanged();
}

void OutputMapDialog::onLinkCellChanged(int, int)
{
    // Combos handle edits; ignore item text edits.
}

void OutputMapDialog::syncFromUi()
{
    const bool resolumeImported = m_map->hasResolumeImport();
    QVector<OutputSlice> previous = m_map->slices();
    m_map->setOutputSize(m_outW->value(), m_outH->value());
    m_map->clear();
    for (int i = 0; i < m_table->rowCount(); ++i) {
        OutputSlice s;
        if (i < previous.size())
            s = previous[i];
        s.name = m_table->item(i, 0) ? m_table->item(i, 0)->text()
                                     : QStringLiteral("Slice %1").arg(i + 1);
        auto cell = [&](int c) {
            return m_table->item(i, c) ? m_table->item(i, c)->text().toDouble() : 0.0;
        };
        s.inputRect = QRectF(cell(1), cell(2), cell(3), cell(4));
        s.outputRect = QRectF(cell(5), cell(6), cell(7), cell(8));
        if (!s.useWarp)
            s.syncCornersFromRect();
        m_map->addSlice(s);
    }
    if (resolumeImported || m_map->slices().size() > 1)
        m_map->setResolumeImported(true);
}

void OutputMapDialog::onSelectionChanged()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_map->slices().size()) {
        m_warpEditor->setSlice(nullptr);
        m_sliceLabel->setText(QStringLiteral("Slice: —"));
        return;
    }
    syncFromUi();
    if (row >= m_map->slices().size())
        return;

    OutputSlice &s = m_map->slices()[row];
    m_updating = true;
    m_sliceLabel->setText(QStringLiteral("Slice: %1").arg(s.name));
    m_useWarp->setChecked(s.useWarp);
    m_softL->setValue(s.softLeft);
    m_softR->setValue(s.softRight);
    m_softT->setValue(s.softTop);
    m_softB->setValue(s.softBottom);
    m_updating = false;
    m_warpEditor->setOutputSize(m_outW->value(), m_outH->value());
    m_warpEditor->setSlice(&s);
}

void OutputMapDialog::onSoftChanged()
{
    if (m_updating)
        return;
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_map->slices().size())
        return;
    auto &s = m_map->slices()[row];
    s.softLeft = static_cast<float>(m_softL->value());
    s.softRight = static_cast<float>(m_softR->value());
    s.softTop = static_cast<float>(m_softT->value());
    s.softBottom = static_cast<float>(m_softB->value());
}

void OutputMapDialog::onWarpToggled(bool on)
{
    if (m_updating)
        return;
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_map->slices().size())
        return;
    auto &s = m_map->slices()[row];
    s.useWarp = on;
    if (!on)
        s.syncCornersFromRect();
    m_warpEditor->refresh();
}

void OutputMapDialog::onResetCorners()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_map->slices().size())
        return;
    syncFromUi();
    if (row >= m_map->slices().size())
        return;
    m_map->slices()[row].syncCornersFromRect();
    m_map->slices()[row].useWarp = m_useWarp->isChecked();
    m_warpEditor->setSlice(&m_map->slices()[row]);
}

void OutputMapDialog::onAdd()
{
    syncFromUi();
    OutputSlice s;
    s.name = QStringLiteral("Slice %1").arg(m_map->slices().size() + 1);
    s.inputRect = QRectF(0, 0, m_canvasW / 2.0, m_canvasH / 2.0);
    s.outputRect = QRectF(0, 0, m_outW->value() / 2.0, m_outH->value() / 2.0);
    s.syncCornersFromRect();
    m_map->addSlice(s);
    reloadTable();
    m_table->selectRow(m_table->rowCount() - 1);
}

void OutputMapDialog::onRemove()
{
    syncFromUi();
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_map->slices().size())
        return;
    m_map->slices().removeAt(row);
    reloadTable();
}

void OutputMapDialog::onReset()
{
    m_map->setOutputSize(m_outW->value(), m_outH->value());
    m_map->resetFullscreen(m_canvasW, m_canvasH);
    reloadTable();
    if (m_table->rowCount() > 0)
        m_table->selectRow(0);
}

void OutputMapDialog::onImportResolume()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Advanced Output Resolume"), QString(),
        QStringLiteral("Resolume Advanced Output (*.xml *.rslm);;XML (*.xml);;Tous (*.*)"));
    if (path.isEmpty())
        return;
    QString err;
    if (!ResolumeXmlImport::importFile(path, m_map, &err)) {
        QMessageBox::warning(this, QStringLiteral("Import Resolume"), err);
        return;
    }
    for (auto &s : m_map->slices())
        s.syncCornersFromRect();
    m_outW->setValue(m_map->outputSize().width());
    m_outH->setValue(m_map->outputSize().height());
    m_warpEditor->setOutputSize(m_outW->value(), m_outH->value());
    reloadTable();
    QMessageBox::information(
        this, QStringLiteral("Import Resolume"),
        QStringLiteral("%1 slice(s) importée(s). Onglet « Modules ↔ Slices » ou menu Slice "
                       "du panneau clip pour lier.")
            .arg(m_map->slices().size()));
}

void OutputMapDialog::onExportJson()
{
    syncFromUi();
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Output Map"), QStringLiteral("output_map.json"),
        QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(m_map->toJson()).toJson(QJsonDocument::Indented));
}

void OutputMapDialog::onImportJson()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Output Map"), QString(), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    m_map->fromJson(doc.object());
    m_outW->setValue(m_map->outputSize().width());
    m_outH->setValue(m_map->outputSize().height());
    m_warpEditor->setOutputSize(m_outW->value(), m_outH->value());
    reloadTable();
}
