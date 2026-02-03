/*
 * 文件名: paramselectdialog.cpp
 * 文件作用: 参数选择配置对话框的具体实现文件
 * 修改记录:
 * 1. 实现了"重置默认参数"和"参数上下限更新"的功能逻辑。
 * 2. 增加了拟合时间的初始化和获取。
 */

#include "paramselectdialog.h"
#include "ui_paramselectdialog.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QDebug>
#include <QMessageBox>

// 自定义 SpinBox 以去除末尾多余的 0
class SmartDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit SmartDoubleSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {}
    QString textFromValue(double value) const override {
        return QString::number(value, 'g', decimals());
    }
};

// 构造函数
ParamSelectDialog::ParamSelectDialog(const QList<FitParameter> &params,
                                     ModelManager::ModelType modelType,
                                     double fitTime,
                                     QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamSelectDialog),
    m_params(params),
    m_modelType(modelType)
{
    ui->setupUi(this);
    this->setWindowTitle("拟合参数配置");

    // 初始化拟合时间输入框
    ui->spinTimeMax->setValue(fitTime);

    // 连接信号
    connect(ui->btnOk, &QPushButton::clicked, this, &ParamSelectDialog::onConfirm);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ParamSelectDialog::onCancel);

    // [新增] 连接功能按钮
    connect(ui->btnResetDefaults, &QPushButton::clicked, this, &ParamSelectDialog::onResetParams);
    connect(ui->btnAutoLimits, &QPushButton::clicked, this, &ParamSelectDialog::onAutoLimits);

    ui->btnCancel->setAutoDefault(false);

    // 初始化表格
    initTable();
}

ParamSelectDialog::~ParamSelectDialog()
{
    delete ui;
}

bool ParamSelectDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        if (qobject_cast<QAbstractSpinBox*>(obj)) {
            return true; // 拦截输入框滚轮
        }
    }
    return QDialog::eventFilter(obj, event);
}

// [新增] 重置默认参数槽函数
void ParamSelectDialog::onResetParams()
{
    if (QMessageBox::question(this, "确认", "确定要重置为该模型的默认参数吗？当前修改将丢失。") != QMessageBox::Yes) {
        return;
    }

    // 调用静态方法生成默认参数
    m_params = FittingParameterChart::generateDefaultParams(m_modelType);

    // 调用静态方法计算上下限
    FittingParameterChart::adjustLimits(m_params);

    // 重新生成表格
    initTable();
}

// [新增] 自动更新限值槽函数
void ParamSelectDialog::onAutoLimits()
{
    // 先收集当前界面上的值
    collectData();

    // 根据当前值计算上下限
    FittingParameterChart::adjustLimits(m_params);

    // 刷新表格
    initTable();

    QMessageBox::information(this, "提示", "参数上下限及滚轮步长已根据当前值更新。");
}

void ParamSelectDialog::initTable()
{
    ui->tableWidget->clear();
    QStringList headers;
    headers << "显示" << "当前数值" << "单位" << "参数名称" << "拟合变量" << "下限" << "上限" << "滚轮步长";
    ui->tableWidget->setColumnCount(headers.size());
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->setRowCount(m_params.size());

    QString checkBoxStyle =
        "QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #cccccc; border-radius: 3px; background-color: white; }"
        "QCheckBox::indicator:checked { background-color: #0078d7; border-color: #0078d7; }"
        "QCheckBox::indicator:hover { border-color: #0078d7; }";

    for(int i = 0; i < m_params.size(); ++i) {
        const FitParameter& p = m_params[i];

        // 0: Visible
        QWidget* pWidgetVis = new QWidget();
        QHBoxLayout* pLayoutVis = new QHBoxLayout(pWidgetVis);
        QCheckBox* chkVis = new QCheckBox();
        chkVis->setChecked(p.isVisible);
        chkVis->setStyleSheet(checkBoxStyle);
        pLayoutVis->addWidget(chkVis);
        pLayoutVis->setAlignment(Qt::AlignCenter);
        pLayoutVis->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 0, pWidgetVis);

        // 1: Value
        SmartDoubleSpinBox* spinVal = new SmartDoubleSpinBox();
        spinVal->setRange(-9e9, 9e9);
        spinVal->setDecimals(10);
        spinVal->setValue(p.value);
        spinVal->setFrame(false);
        spinVal->installEventFilter(this);
        ui->tableWidget->setCellWidget(i, 1, spinVal);

        // 2: Unit
        QString dummy, dummy2, dummy3, unitStr;
        FittingParameterChart::getParamDisplayInfo(p.name, dummy, dummy2, dummy3, unitStr);
        if(unitStr == "无因次" || unitStr == "小数") unitStr = "-";
        QTableWidgetItem* unitItem = new QTableWidgetItem(unitStr);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 2, unitItem);

        // 3: Name
        QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);
        QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, p.name);
        ui->tableWidget->setItem(i, 3, nameItem);

        // 4: Fit
        QWidget* pWidgetFit = new QWidget();
        QHBoxLayout* pLayoutFit = new QHBoxLayout(pWidgetFit);
        QCheckBox* chkFit = new QCheckBox();
        chkFit->setChecked(p.isFit);
        chkFit->setStyleSheet(checkBoxStyle);
        pLayoutFit->addWidget(chkFit);
        pLayoutFit->setAlignment(Qt::AlignCenter);
        pLayoutFit->setContentsMargins(0,0,0,0);
        if (p.name == "LfD") { chkFit->setEnabled(false); chkFit->setChecked(false); }
        ui->tableWidget->setCellWidget(i, 4, pWidgetFit);

        connect(chkFit, &QCheckBox::checkStateChanged, [chkVis](Qt::CheckState state){
            if (state == Qt::Checked) {
                chkVis->setChecked(true);
                chkVis->setEnabled(false);
                chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #ccc; border-radius: 3px; background-color: #e0e0e0; } "
                                      "QCheckBox::indicator:checked { background-color: #80bbeb; border-color: #80bbeb; }");
            } else {
                chkVis->setEnabled(true);
                chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #cccccc; border-radius: 3px; background-color: white; }"
                                      "QCheckBox::indicator:checked { background-color: #0078d7; border-color: #0078d7; }"
                                      "QCheckBox::indicator:hover { border-color: #0078d7; }");
            }
        });

        if (p.isFit) {
            chkVis->setChecked(true);
            chkVis->setEnabled(false);
            chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #ccc; border-radius: 3px; background-color: #e0e0e0; } "
                                  "QCheckBox::indicator:checked { background-color: #80bbeb; border-color: #80bbeb; }");
        }

        // 5: Min
        SmartDoubleSpinBox* spinMin = new SmartDoubleSpinBox();
        spinMin->setRange(-9e9, 9e9);
        spinMin->setDecimals(10);
        spinMin->setValue(p.min);
        spinMin->setFrame(false);
        spinMin->installEventFilter(this);
        ui->tableWidget->setCellWidget(i, 5, spinMin);

        // 6: Max
        SmartDoubleSpinBox* spinMax = new SmartDoubleSpinBox();
        spinMax->setRange(-9e9, 9e9);
        spinMax->setDecimals(10);
        spinMax->setValue(p.max);
        spinMax->setFrame(false);
        spinMax->installEventFilter(this);
        ui->tableWidget->setCellWidget(i, 6, spinMax);

        // 7: Step
        SmartDoubleSpinBox* spinStep = new SmartDoubleSpinBox();
        spinStep->setRange(0.0, 10000.0);
        spinStep->setDecimals(10);
        spinStep->setValue(p.step);
        spinStep->setFrame(false);
        spinStep->installEventFilter(this);
        ui->tableWidget->setCellWidget(i, 7, spinStep);
    }
    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
}

void ParamSelectDialog::collectData()
{
    for(int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        if(i >= m_params.size()) break;

        QWidget* wVis = ui->tableWidget->cellWidget(i, 0);
        if (wVis) { QCheckBox* cb = wVis->findChild<QCheckBox*>(); if(cb) m_params[i].isVisible = cb->isChecked(); }

        QDoubleSpinBox* spinVal = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 1));
        if(spinVal) m_params[i].value = spinVal->value();

        QWidget* wFit = ui->tableWidget->cellWidget(i, 4);
        if (wFit) { QCheckBox* cb = wFit->findChild<QCheckBox*>(); if(cb) m_params[i].isFit = cb->isChecked(); }

        QDoubleSpinBox* spinMin = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 5));
        if(spinMin) m_params[i].min = spinMin->value();

        QDoubleSpinBox* spinMax = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 6));
        if(spinMax) m_params[i].max = spinMax->value();

        QDoubleSpinBox* spinStep = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 7));
        if(spinStep) m_params[i].step = spinStep->value();
    }
}

QList<FitParameter> ParamSelectDialog::getUpdatedParams() const
{
    return m_params;
}

double ParamSelectDialog::getFittingTime() const
{
    return ui->spinTimeMax->value();
}

void ParamSelectDialog::onConfirm()
{
    collectData();
    accept();
}

void ParamSelectDialog::onCancel()
{
    reject();
}
