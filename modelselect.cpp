/*
 * 文件名: modelselect.cpp
 * 文件作用: 模型选择对话框逻辑实现
 * 修改记录:
 * 1. [联动] 实现了储层模型与内外区模型的动态联动逻辑。
 * 2. [新增] 增加了夹层型、页岩型、混积型径向复合模型的选项逻辑。
 * 3. [匹配] 完成了夹层型径向复合模型(model19-36)的代码匹配逻辑。
 * 4. [回显] 支持1-36号模型的反向解析和回显。
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
    // 设置窗口默认字体样式
    this->setStyleSheet("QWidget { color: black; font-family: Arial; }");

    // 初始化固定的选项内容
    initOptions();

    // 信号连接
    // 当储层模型变化时，首先需要更新内外区模型的选项列表
    connect(ui->comboReservoirModel, SIGNAL(currentIndexChanged(int)), this, SLOT(updateInnerOuterOptions()));

    // 所有影响模型确定的下拉框变化时，重新计算选中的模型代码
    connect(ui->comboWellModel, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboReservoirModel, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboBoundary, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboStorage, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboInnerOuter, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));

    // 按钮盒信号处理
    disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ModelSelect::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 根据初始状态计算一次模型
    onSelectionChanged();
}

ModelSelect::~ModelSelect()
{
    delete ui;
}

void ModelSelect::initOptions()
{
    m_isInitializing = true;

    // 清空所有下拉框
    ui->comboWellModel->clear();
    ui->comboReservoirModel->clear();
    ui->comboBoundary->clear();
    ui->comboStorage->clear();
    ui->comboInnerOuter->clear();

    // 1. 井模型 (目前只有一种)
    ui->comboWellModel->addItem("压裂水平井", "FracHorizontal");

    // 2. 储层模型 (添加所有四种类型)
    ui->comboReservoirModel->addItem("径向复合模型", "RadialComposite");
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
    // 初始状态下根据默认的储层模型(径向复合)填充选项
    // 这里暂时不填，马上调用 updateInnerOuterOptions 进行填充

    // 设置默认选中项
    ui->comboWellModel->setCurrentIndex(0);
    ui->comboReservoirModel->setCurrentIndex(0); // 默认选中径向复合
    ui->comboBoundary->setCurrentIndex(0);
    ui->comboStorage->setCurrentIndex(0);

    m_isInitializing = false;

    // 立即根据当前储层模型初始化内外区下拉框
    updateInnerOuterOptions();
}

// [新增功能] 根据储层模型的选择，动态更新内外区模型的选项
void ModelSelect::updateInnerOuterOptions()
{
    // 临时阻塞comboInnerOuter的信号，防止清空和添加项时频繁触发onSelectionChanged
    bool oldState = ui->comboInnerOuter->blockSignals(true);

    ui->comboInnerOuter->clear();
    QString currentRes = ui->comboReservoirModel->currentData().toString();

    if (currentRes == "RadialComposite") {
        // 1. 径向复合模型
        ui->comboInnerOuter->addItem("双重孔隙+双重孔隙", "Dual_Dual");
        ui->comboInnerOuter->addItem("均质+均质", "Homo_Homo");
        ui->comboInnerOuter->addItem("双重孔隙+均质", "Dual_Homo");
    }
    else if (currentRes == "InterlayerComposite") {
        // 2. 夹层型径向复合模型
        ui->comboInnerOuter->addItem("夹层型+夹层型", "Interlayer_Interlayer");
        ui->comboInnerOuter->addItem("夹层型+均质", "Interlayer_Homo");
        ui->comboInnerOuter->addItem("夹层型+双重孔隙", "Interlayer_Dual");
    }
    else if (currentRes == "ShaleComposite") {
        // 3. 页岩型径向复合模型
        ui->comboInnerOuter->addItem("页岩型+页岩型", "Shale_Shale");
        ui->comboInnerOuter->addItem("页岩型+均质", "Shale_Homo");
        ui->comboInnerOuter->addItem("页岩型+双重孔隙", "Shale_Dual");
    }
    else if (currentRes == "MixedComposite") {
        // 4. 混积型径向复合模型
        ui->comboInnerOuter->addItem("混积型+混积型", "Mixed_Mixed");
        ui->comboInnerOuter->addItem("混积型+均质", "Mixed_Homo");
        ui->comboInnerOuter->addItem("混积型+双重孔隙", "Mixed_Dual");
    }

    // 默认选中第一个
    if (ui->comboInnerOuter->count() > 0) {
        ui->comboInnerOuter->setCurrentIndex(0);
    }

    // 恢复信号阻塞状态
    ui->comboInnerOuter->blockSignals(oldState);

    // 显示或隐藏内外区选项（目前都是复合模型，应该总是显示，但保留逻辑以防万一）
    bool isComposite = (currentRes == "RadialComposite" || currentRes == "InterlayerComposite" ||
                        currentRes == "ShaleComposite" || currentRes == "MixedComposite");
    ui->label_InnerOuter->setVisible(isComposite);
    ui->comboInnerOuter->setVisible(isComposite);
}

void ModelSelect::setCurrentModelCode(const QString& code)
{
    m_isInitializing = true;

    QString numStr = code;
    numStr.remove("modelwidget");
    int id = numStr.toInt();

    // 根据ID反向推导各个下拉框应该选中的值
    if (id >= 1) {
        // 1. 井 (目前都是压裂水平井)
        int idxWell = ui->comboWellModel->findData("FracHorizontal");
        if (idxWell >= 0) ui->comboWellModel->setCurrentIndex(idxWell);

        // 2. 储层 & 5. 内外区
        // 需要根据ID范围判断储层类型
        QString resData;
        QString ioData;

        if (id >= 1 && id <= 18) {
            // 径向复合模型 (1-18)
            resData = "RadialComposite";
            if (id <= 6) ioData = "Dual_Dual";
            else if (id <= 12) ioData = "Homo_Homo";
            else ioData = "Dual_Homo";
        }
        else if (id >= 19 && id <= 36) {
            // 夹层型径向复合模型 (19-36)
            resData = "InterlayerComposite";
            if (id <= 24) ioData = "Interlayer_Interlayer";
            else if (id <= 30) ioData = "Interlayer_Homo";
            else ioData = "Interlayer_Dual";
        }
        // 后续页岩型和混积型如果有ID，可以在这里继续添加 else if

        // 设置储层下拉框
        int idxRes = ui->comboReservoirModel->findData(resData);
        if (idxRes >= 0) {
            ui->comboReservoirModel->setCurrentIndex(idxRes);
            // 关键：切换储层后，必须手动触发更新内外区列表，否则列表里没有对应的选项
            updateInnerOuterOptions();
        }

        // 3. 边界条件
        // 规律：每6个一组。
        // 1,2,7,8,13,14... -> Infinite
        // 3,4,9,10,15,16... -> Closed
        // 5,6,11,12,17,18... -> ConstantPressure
        // (id - 1) % 6 的结果：0,1 -> Infinite; 2,3 -> Closed; 4,5 -> ConstantPressure
        QString bndData;
        int rem = (id - 1) % 6;
        if (rem == 0 || rem == 1) bndData = "Infinite";
        else if (rem == 2 || rem == 3) bndData = "Closed";
        else bndData = "ConstantPressure";

        int idxBnd = ui->comboBoundary->findData(bndData);
        if (idxBnd >= 0) ui->comboBoundary->setCurrentIndex(idxBnd);

        // 4. 井储 (规律：奇数考虑，偶数不考虑)
        QString storeData = (id % 2 != 0) ? "Consider" : "Ignore";
        int idxStore = ui->comboStorage->findData(storeData);
        if (idxStore >= 0) ui->comboStorage->setCurrentIndex(idxStore);

        // 设置内外区下拉框 (此时列表已被 updateInnerOuterOptions 更新)
        int idxIo = ui->comboInnerOuter->findData(ioData);
        if (idxIo >= 0) ui->comboInnerOuter->setCurrentIndex(idxIo);
    }

    m_isInitializing = false;
    // 最后触发一次变更逻辑，更新界面显示的名称和代码
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

    m_selectedModelCode = "";
    m_selectedModelName = "";

    // === 1. 径向复合模型 (Model 1-18) ===
    if (well == "FracHorizontal" && res == "RadialComposite") {
        if (io == "Dual_Dual") { // 1-6
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
        else if (io == "Homo_Homo") { // 7-12
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
        else if (io == "Dual_Homo") { // 13-18
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
    // === 2. 夹层型径向复合模型 (Model 19-36) ===
    else if (well == "FracHorizontal" && res == "InterlayerComposite") {
        if (io == "Interlayer_Interlayer") { // 19-24
            if (bnd == "Infinite") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget19"; m_selectedModelName = "压力水平井夹层型模型1"; }
                else                     { m_selectedModelCode = "modelwidget20"; m_selectedModelName = "压力水平井夹层型模型2"; }
            } else if (bnd == "Closed") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget21"; m_selectedModelName = "压力水平井夹层型模型3"; }
                else                     { m_selectedModelCode = "modelwidget22"; m_selectedModelName = "压力水平井夹层型模型4"; }
            } else if (bnd == "ConstantPressure") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget23"; m_selectedModelName = "压力水平井夹层型模型5"; }
                else                     { m_selectedModelCode = "modelwidget24"; m_selectedModelName = "压力水平井夹层型模型6"; }
            }
        }
        else if (io == "Interlayer_Homo") { // 25-30
            if (bnd == "Infinite") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget25"; m_selectedModelName = "压力水平井夹层型模型7"; }
                else                     { m_selectedModelCode = "modelwidget26"; m_selectedModelName = "压力水平井夹层型模型8"; }
            } else if (bnd == "Closed") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget27"; m_selectedModelName = "压力水平井夹层型模型9"; }
                else                     { m_selectedModelCode = "modelwidget28"; m_selectedModelName = "压力水平井夹层型模型10"; }
            } else if (bnd == "ConstantPressure") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget29"; m_selectedModelName = "压力水平井夹层型模型11"; }
                else                     { m_selectedModelCode = "modelwidget30"; m_selectedModelName = "压力水平井夹层型模型12"; }
            }
        }
        else if (io == "Interlayer_Dual") { // 31-36
            if (bnd == "Infinite") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget31"; m_selectedModelName = "压力水平井夹层型模型13"; }
                else                     { m_selectedModelCode = "modelwidget32"; m_selectedModelName = "压力水平井夹层型模型14"; }
            } else if (bnd == "Closed") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget33"; m_selectedModelName = "压力水平井夹层型模型15"; }
                else                     { m_selectedModelCode = "modelwidget34"; m_selectedModelName = "压力水平井夹层型模型16"; }
            } else if (bnd == "ConstantPressure") {
                if (store == "Consider") { m_selectedModelCode = "modelwidget35"; m_selectedModelName = "压力水平井夹层型模型17"; }
                else                     { m_selectedModelCode = "modelwidget36"; m_selectedModelName = "压力水平井夹层型模型18"; }
            }
        }
    }
    // === 3. 页岩型 / 4. 混积型 (尚未实现) ===
    // 逻辑保留，待后续添加模型ID
    else {
        // 暂未实现
    }

    bool isValid = !m_selectedModelCode.isEmpty();

    // 更新界面显示
    if (isValid) {
        ui->label_ModelName->setText(m_selectedModelName);
        ui->label_ModelName->setStyleSheet("color: black; font-weight: bold; font-size: 14px;");
    } else {
        ui->label_ModelName->setText("该组合暂无已实现模型");
        ui->label_ModelName->setStyleSheet("color: red; font-weight: normal;");
    }

    // 更新确认按钮状态
    QPushButton* okBtn = ui->buttonBox->button(QDialogButtonBox::Ok);
    if(okBtn) okBtn->setEnabled(isValid);
}

void ModelSelect::onAccepted() {
    if (!m_selectedModelCode.isEmpty()) accept();
}

QString ModelSelect::getSelectedModelCode() const { return m_selectedModelCode; }
QString ModelSelect::getSelectedModelName() const { return m_selectedModelName; }
