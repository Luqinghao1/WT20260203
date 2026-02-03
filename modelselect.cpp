/*
 * 文件名: modelselect.cpp
 * 文件作用: 模型选择对话框逻辑实现
 * 修改记录:
 * 1. [新增] 增加"双重孔隙+均质"选项。
 * 2. [新增] 支持模型13-18的选择匹配。
 * 3. [回显] 支持1-18号模型的正确回显。
 */

#include "modelselect.h"
#include "ui_modelselect.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <QDebug>

ModelSelect::ModelSelect(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ModelSelect),
    m_isInitializing(false)
{
    ui->setupUi(this);
    this->setStyleSheet("QWidget { color: black; font-family: Arial; }");

    initOptions();

    connect(ui->comboWellModel, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboReservoirModel, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboBoundary, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboStorage, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboInnerOuter, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));

    disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ModelSelect::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    onSelectionChanged();
}

ModelSelect::~ModelSelect()
{
    delete ui;
}

void ModelSelect::initOptions()
{
    m_isInitializing = true;

    ui->comboWellModel->clear();
    ui->comboReservoirModel->clear();
    ui->comboBoundary->clear();
    ui->comboStorage->clear();
    ui->comboInnerOuter->clear();

    // 1. 井模型
    ui->comboWellModel->addItem("压裂水平井", "FracHorizontal");

    // 2. 储层模型
    ui->comboReservoirModel->addItem("径向复合模型", "RadialComposite");
    // 保留其他预留项...
    ui->comboReservoirModel->addItem("夹层型径向复合模型", "InterlayerComposite");
    ui->comboReservoirModel->addItem("页岩型径向复合模型", "ShaleComposite");
    ui->comboReservoirModel->addItem("混积型径向复合模型", "MixedComposite");

    // 3. 边界条件
    ui->comboBoundary->addItem("无限大外边界", "Infinite");
    ui->comboBoundary->addItem("封闭边界", "Closed");
    ui->comboBoundary->addItem("定压边界", "ConstantPressure");

    // 4. 井储表皮
    ui->comboStorage->addItem("考虑井储表皮", "Consider");
    ui->comboStorage->addItem("不考虑井储表皮", "Ignore");

    // 5. 内外区模型
    ui->comboInnerOuter->addItem("双重孔隙+双重孔隙", "Dual_Dual");
    ui->comboInnerOuter->addItem("均质+均质", "Homo_Homo");
    // [新增]
    ui->comboInnerOuter->addItem("双重孔隙+均质", "Dual_Homo");

    // 默认选中
    ui->comboWellModel->setCurrentIndex(0);
    ui->comboReservoirModel->setCurrentIndex(0);
    ui->comboBoundary->setCurrentIndex(0);
    ui->comboStorage->setCurrentIndex(0);
    ui->comboInnerOuter->setCurrentIndex(0);

    m_isInitializing = false;
}

void ModelSelect::setCurrentModelCode(const QString& code)
{
    m_isInitializing = true;

    QString numStr = code;
    numStr.remove("modelwidget");
    int id = numStr.toInt();

    if (id >= 1 && id <= 18) {
        // 1. 井
        int idxWell = ui->comboWellModel->findData("FracHorizontal");
        if (idxWell >= 0) ui->comboWellModel->setCurrentIndex(idxWell);

        // 2. 储层
        int idxRes = ui->comboReservoirModel->findData("RadialComposite");
        if (idxRes >= 0) ui->comboReservoirModel->setCurrentIndex(idxRes);

        // 3. 边界
        // Infinite: 1,2, 7,8, 13,14
        // Closed: 3,4, 9,10, 15,16
        // ConstP: 5,6, 11,12, 17,18
        QString bndData;
        int rem = (id - 1) % 6; // 0..5
        if (rem == 0 || rem == 1) bndData = "Infinite";
        else if (rem == 2 || rem == 3) bndData = "Closed";
        else bndData = "ConstantPressure";
        int idxBnd = ui->comboBoundary->findData(bndData);
        if (idxBnd >= 0) ui->comboBoundary->setCurrentIndex(idxBnd);

        // 4. 井储 (偶数ID为Consider)
        QString storeData = (id % 2 != 0) ? "Consider" : "Ignore";
        int idxStore = ui->comboStorage->findData(storeData);
        if (idxStore >= 0) ui->comboStorage->setCurrentIndex(idxStore);

        // 5. 内外区
        // 1-6 -> Dual_Dual
        // 7-12 -> Homo_Homo
        // 13-18 -> Dual_Homo
        QString ioData;
        if (id <= 6) ioData = "Dual_Dual";
        else if (id <= 12) ioData = "Homo_Homo";
        else ioData = "Dual_Homo";

        int idxIo = ui->comboInnerOuter->findData(ioData);
        if (idxIo >= 0) ui->comboInnerOuter->setCurrentIndex(idxIo);
    }

    m_isInitializing = false;
    onSelectionChanged();
}

void ModelSelect::onSelectionChanged()
{
    if (m_isInitializing) return;

    QString well = ui->comboWellModel->currentData().toString();
    QString res = ui->comboReservoirModel->currentData().toString();
    QString bnd = ui->comboBoundary->currentData().toString();
    QString store = ui->comboStorage->currentData().toString();
    QString io = ui->comboInnerOuter->currentData().toString();

    bool isComposite = (res == "RadialComposite" || res == "InterlayerComposite" ||
                        res == "ShaleComposite" || res == "MixedComposite");
    ui->label_InnerOuter->setVisible(isComposite);
    ui->comboInnerOuter->setVisible(isComposite);

    bool isValid = false;
    m_selectedModelCode = "";
    m_selectedModelName = "";

    if (well == "FracHorizontal" && res == "RadialComposite") {

        // --- 双重孔隙+双重孔隙 (1-6) ---
        if (io == "Dual_Dual") {
            if (bnd == "Infinite") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget1"; m_selectedModelName = "压裂水平井径向复合模型1"; }
                else                     { m_selectedModelCode = "modelwidget2"; m_selectedModelName = "压裂水平井径向复合模型2"; }
            } else if (bnd == "Closed") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget3"; m_selectedModelName = "压裂水平井径向复合模型3"; }
                else                     { m_selectedModelCode = "modelwidget4"; m_selectedModelName = "压裂水平井径向复合模型4"; }
            } else if (bnd == "ConstantPressure") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget5"; m_selectedModelName = "压裂水平井径向复合模型5"; }
                else                     { m_selectedModelCode = "modelwidget6"; m_selectedModelName = "压裂水平井径向复合模型6"; }
            }
        }
        // --- 均质+均质 (7-12) ---
        else if (io == "Homo_Homo") {
            if (bnd == "Infinite") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget7"; m_selectedModelName = "压裂水平井径向复合模型7"; }
                else                     { m_selectedModelCode = "modelwidget8"; m_selectedModelName = "压裂水平井径向复合模型8"; }
            } else if (bnd == "Closed") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget9"; m_selectedModelName = "压裂水平井径向复合模型9"; }
                else                     { m_selectedModelCode = "modelwidget10"; m_selectedModelName = "压裂水平井径向复合模型10"; }
            } else if (bnd == "ConstantPressure") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget11"; m_selectedModelName = "压裂水平井径向复合模型11"; }
                else                     { m_selectedModelCode = "modelwidget12"; m_selectedModelName = "压裂水平井径向复合模型12"; }
            }
        }
        // --- 双重孔隙+均质 (13-18) [新增] ---
        else if (io == "Dual_Homo") {
            if (bnd == "Infinite") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget13"; m_selectedModelName = "压裂水平井径向复合模型13"; }
                else                     { m_selectedModelCode = "modelwidget14"; m_selectedModelName = "压裂水平井径向复合模型14"; }
            } else if (bnd == "Closed") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget15"; m_selectedModelName = "压裂水平井径向复合模型15"; }
                else                     { m_selectedModelCode = "modelwidget16"; m_selectedModelName = "压裂水平井径向复合模型16"; }
            } else if (bnd == "ConstantPressure") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget17"; m_selectedModelName = "压裂水平井径向复合模型17"; }
                else                     { m_selectedModelCode = "modelwidget18"; m_selectedModelName = "压裂水平井径向复合模型18"; }
            }
        }
    }

    isValid = !m_selectedModelCode.isEmpty();

    if (isValid) {
        ui->label_ModelName->setText(m_selectedModelName);
        ui->label_ModelName->setStyleSheet("color: black; font-weight: bold; font-size: 14px;");
    } else {
        ui->label_ModelName->setText("该组合暂无已实现模型");
        ui->label_ModelName->setStyleSheet("color: red; font-weight: normal;");
    }

    QPushButton* okBtn = ui->buttonBox->button(QDialogButtonBox::Ok);
    if(okBtn) okBtn->setEnabled(isValid);
}

void ModelSelect::onAccepted() {
    if (!m_selectedModelCode.isEmpty()) accept();
}

QString ModelSelect::getSelectedModelCode() const { return m_selectedModelCode; }
QString ModelSelect::getSelectedModelName() const { return m_selectedModelName; }
