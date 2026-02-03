/*
 * 文件名: wt_modelwidget.cpp
 * 文件作用: 压裂水平井复合页岩油模型界面类实现
 * 功能描述:
 * 1. 界面初始化：根据模型类型动态调整参数输入框的显示/隐藏 (如内/外区参数)。
 * 2. 求解器集成：根据 ID 范围 (0-17 或 18-35) 实例化并调用对应的数学模型。
 * 3. 业务逻辑：处理计算请求、参数敏感性分析、结果绘图与导出。
 */

#include "wt_modelwidget.h"
#include "ui_wt_modelwidget.h"
#include "modelmanager.h"
#include "modelparameter.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <cmath>

WT_ModelWidget::WT_ModelWidget(ModelType type, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WT_ModelWidget)
    , m_type(type)
    , m_solver1(nullptr)
    , m_solver2(nullptr)
    , m_highPrecision(true)
{
    ui->setupUi(this);

    // [修改] 根据 ID 范围实例化对应的求解器
    if (m_type >= 0 && m_type <= 17) {
        m_solver1 = new ModelSolver01_06((ModelSolver01_06::ModelType)m_type);
    }
    else if (m_type >= 18 && m_type <= 35) {
        // ModelSolver19_36 的枚举从 0 开始
        m_solver2 = new ModelSolver19_36((ModelSolver19_36::ModelType)(m_type - 18));
    }

    // 初始化曲线颜色列表
    m_colorList = { Qt::red, Qt::blue, QColor(0,180,0), Qt::magenta, QColor(255,140,0), Qt::cyan };

    // 设置 Splitter 初始比例
    QList<int> sizes;
    sizes << 240 << 960;
    ui->splitter->setSizes(sizes);
    ui->splitter->setCollapsible(0, false);

    // 设置按钮文本
    ui->btnSelectModel->setText(getModelName());

    initUi();
    initChart();
    setupConnections();

    // 初始化参数显示
    onResetParameters();
}

WT_ModelWidget::~WT_ModelWidget()
{
    if(m_solver1) delete m_solver1;
    if(m_solver2) delete m_solver2;
    delete ui;
}

QString WT_ModelWidget::getModelName() const {
    if (m_solver1) return ModelSolver01_06::getModelName((ModelSolver01_06::ModelType)m_type, false);
    if (m_solver2) return ModelSolver19_36::getModelName((ModelSolver19_36::ModelType)(m_type - 18), false);
    return "未知模型";
}

// 转发给 Solver 进行计算
WT_ModelWidget::ModelCurveData WT_ModelWidget::calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    if (m_solver1) {
        return m_solver1->calculateTheoreticalCurve(params, providedTime);
    }
    if (m_solver2) {
        return m_solver2->calculateTheoreticalCurve(params, providedTime);
    }
    return ModelCurveData();
}

void WT_ModelWidget::setHighPrecision(bool high)
{
    m_highPrecision = high;
    if (m_solver1) m_solver1->setHighPrecision(high);
    if (m_solver2) m_solver2->setHighPrecision(high);
}

void WT_ModelWidget::initUi() {
    // 1. 标签文本更新
    if(ui->label_km) ui->label_km->setText("流度比 M12");
    if(ui->label_rmD) ui->label_rmD->setText("复合半径 rm (m)");
    if(ui->label_reD) ui->label_reD->setText("外区半径 re (m)");
    if(ui->label_cD) ui->label_cD->setText("井筒储集 C (m³/MPa)");

    // 2. 动态添加新参数输入框 (remda2, eta12)
    QWidget* parentWidget = ui->remda1Edit->parentWidget();
    QGridLayout* layout = qobject_cast<QGridLayout*>(parentWidget->layout());
    QLineEdit* editRemda2 = parentWidget->findChild<QLineEdit*>("remda2Edit");
    QLineEdit* editEta12 = parentWidget->findChild<QLineEdit*>("eta12Edit");

    if (layout) {
        if (!editRemda2) {
            QLabel* labelRemda2 = new QLabel("外区窜流系数 λ<sub>2</sub>:", parentWidget);
            labelRemda2->setObjectName("label_remda2");
            editRemda2 = new QLineEdit(parentWidget);
            editRemda2->setObjectName("remda2Edit");

            QLabel* labelEta12 = new QLabel("导压系数比 η<sub>12</sub>:", parentWidget);
            editEta12 = new QLineEdit(parentWidget);
            editEta12->setObjectName("eta12Edit");

            int row = layout->rowCount();
            layout->addWidget(labelRemda2, row, 0);
            layout->addWidget(editRemda2, row, 1);
            layout->addWidget(labelEta12, row + 1, 0);
            layout->addWidget(editEta12, row + 1, 1);
        }
    }

    // 3. 控制边界参数可见性 (规律: 每6个一组，前2个为无限大)
    // 无限大: id%6 == 0 or 1
    int rem = m_type % 6;
    bool isInfinite = (rem == 0 || rem == 1);

    if (isInfinite) {
        ui->label_reD->setVisible(false);
        ui->reDEdit->setVisible(false);
    } else {
        ui->label_reD->setVisible(true);
        ui->reDEdit->setVisible(true);
    }

    // 4. 控制井储表皮可见性 (偶数ID为考虑)
    bool hasStorage = (m_type % 2 == 0);
    ui->label_cD->setVisible(hasStorage);
    ui->cDEdit->setVisible(hasStorage);
    ui->label_s->setVisible(hasStorage);
    ui->sEdit->setVisible(hasStorage);

    // 5. [核心修改] 介质参数可见性配置
    bool hasInnerParams = false;
    bool hasOuterParams = false;

    if (m_type <= 17) {
        // --- Model 1-18 ---
        // 内区双孔: 1-6 (0-5), 13-18 (12-17)
        if (m_type <= 5 || (m_type >= 12 && m_type <= 17)) hasInnerParams = true;
        // 外区双孔: 1-6 (0-5)
        if (m_type <= 5) hasOuterParams = true;
    }
    else {
        // --- Model 19-36 ---
        // 内区全是夹层型，需要 w1, l1
        hasInnerParams = true;

        int subId = m_type - 18;
        // 19-24(sub 0-5): 外区夹层 -> Need
        // 25-30(sub 6-11): 外区均质 -> No
        // 31-36(sub 12-17): 外区双孔 -> Need
        if (subId <= 5 || subId >= 12) hasOuterParams = true;
    }

    ui->label_omga1->setVisible(hasInnerParams);
    ui->omga1Edit->setVisible(hasInnerParams);
    ui->label_remda1->setVisible(hasInnerParams);
    ui->remda1Edit->setVisible(hasInnerParams);

    ui->label_omga2->setVisible(hasOuterParams);
    ui->omga2Edit->setVisible(hasOuterParams);

    QLabel* labelRemda2 = parentWidget->findChild<QLabel*>("label_remda2");
    if(labelRemda2) labelRemda2->setVisible(hasOuterParams);
    if(editRemda2) editRemda2->setVisible(hasOuterParams);
}

void WT_ModelWidget::initChart() {
    MouseZoom* plot = ui->chartWidget->getPlot();

    plot->setBackground(Qt::white);
    plot->axisRect()->setBackground(Qt::white);

    // 设置对数坐标轴
    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    plot->xAxis->setScaleType(QCPAxis::stLogarithmic); plot->xAxis->setTicker(logTicker);
    plot->yAxis->setScaleType(QCPAxis::stLogarithmic); plot->yAxis->setTicker(logTicker);
    plot->xAxis->setNumberFormat("eb"); plot->xAxis->setNumberPrecision(0);
    plot->yAxis->setNumberFormat("eb"); plot->yAxis->setNumberPrecision(0);

    QFont labelFont("Microsoft YaHei", 10, QFont::Bold);
    QFont tickFont("Microsoft YaHei", 9);
    plot->xAxis->setLabel("时间 Time (h)");
    plot->yAxis->setLabel("压力 & 导数 Pressure & Derivative (MPa)");
    plot->xAxis->setLabelFont(labelFont); plot->yAxis->setLabelFont(labelFont);
    plot->xAxis->setTickLabelFont(tickFont); plot->yAxis->setTickLabelFont(tickFont);

    plot->xAxis2->setVisible(true); plot->yAxis2->setVisible(true);
    plot->xAxis2->setTickLabels(false); plot->yAxis2->setTickLabels(false);
    connect(plot->xAxis, SIGNAL(rangeChanged(QCPRange)), plot->xAxis2, SLOT(setRange(QCPRange)));
    connect(plot->yAxis, SIGNAL(rangeChanged(QCPRange)), plot->yAxis2, SLOT(setRange(QCPRange)));
    plot->xAxis2->setScaleType(QCPAxis::stLogarithmic); plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis2->setTicker(logTicker); plot->yAxis2->setTicker(logTicker);

    plot->xAxis->grid()->setVisible(true); plot->yAxis->grid()->setVisible(true);
    plot->xAxis->grid()->setSubGridVisible(true); plot->yAxis->grid()->setSubGridVisible(true);
    plot->xAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    plot->yAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    plot->xAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));
    plot->yAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));

    plot->xAxis->setRange(1e-3, 1e3); plot->yAxis->setRange(1e-3, 1e2);

    plot->legend->setVisible(true);
    plot->legend->setFont(QFont("Microsoft YaHei", 9));
    plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));

    ui->chartWidget->setTitle("复合页岩油储层试井曲线");
}

void WT_ModelWidget::setupConnections() {
    connect(ui->calculateButton, &QPushButton::clicked, this, &WT_ModelWidget::onCalculateClicked);
    connect(ui->resetButton, &QPushButton::clicked, this, &WT_ModelWidget::onResetParameters);
    connect(ui->chartWidget, &ChartWidget::exportDataTriggered, this, &WT_ModelWidget::onExportData);
    connect(ui->btnExportDataTab, &QPushButton::clicked, this, &WT_ModelWidget::onExportData);

    connect(ui->checkShowPoints, &QCheckBox::toggled, this, &WT_ModelWidget::onShowPointsToggled);

    // 转发模型选择按钮信号
    connect(ui->btnSelectModel, &QPushButton::clicked, this, &WT_ModelWidget::requestModelSelection);
}

QVector<double> WT_ModelWidget::parseInput(const QString& text) {
    QVector<double> values;
    QString cleanText = text;
    cleanText.replace("，", ",");
    QStringList parts = cleanText.split(",", Qt::SkipEmptyParts);
    for(const QString& part : parts) {
        bool ok;
        double v = part.trimmed().toDouble(&ok);
        if(ok) values.append(v);
    }
    if(values.isEmpty()) values.append(0.0);
    return values;
}

void WT_ModelWidget::setInputText(QLineEdit* edit, double value) {
    if(!edit) return;
    edit->setText(QString::number(value, 'g', 8));
}

// 重置参数函数
void WT_ModelWidget::onResetParameters() {
    // 调用 Manager 的统一接口获取默认参数
    ModelManager mgr;
    QMap<QString, double> defaults = mgr.getDefaultParameters(m_type);

    // 1. 基础参数
    setInputText(ui->phiEdit, defaults["phi"]);
    setInputText(ui->hEdit, defaults["h"]);
    setInputText(ui->rwEdit, defaults["rw"]);
    setInputText(ui->muEdit, defaults["mu"]);
    setInputText(ui->BEdit, defaults["B"]);
    setInputText(ui->CtEdit, defaults["Ct"]);
    setInputText(ui->qEdit, defaults["q"]);

    setInputText(ui->tEdit, 1000.0);
    setInputText(ui->pointsEdit, 100);

    // 2. 模型参数
    setInputText(ui->kfEdit, defaults["kf"]);
    setInputText(ui->kmEdit, defaults["M12"]);
    setInputText(ui->LEdit, defaults["L"]);
    setInputText(ui->LfEdit, defaults["Lf"]);
    setInputText(ui->nfEdit, defaults["nf"]);
    setInputText(ui->rmDEdit, defaults["rm"]);

    setInputText(ui->omga1Edit, defaults.value("omega1", 0.0));
    setInputText(ui->omga2Edit, defaults.value("omega2", 0.0));
    setInputText(ui->remda1Edit, defaults.value("lambda1", 0.0));

    QLineEdit* editRemda2 = this->findChild<QLineEdit*>("remda2Edit");
    if(editRemda2) setInputText(editRemda2, defaults.value("lambda2", 0.0));

    QLineEdit* editEta12 = this->findChild<QLineEdit*>("eta12Edit");
    if(editEta12) setInputText(editEta12, defaults.value("eta12", 0.2));

    setInputText(ui->gamaDEdit, defaults.value("gamaD", 0.02));

    if (ui->reDEdit->isVisible()) {
        setInputText(ui->reDEdit, defaults.value("re", 20000.0));
    }

    if (ui->cDEdit->isVisible()) {
        setInputText(ui->cDEdit, 0.1); // CD
        setInputText(ui->sEdit, defaults.value("S", 0.0));
    }
}

void WT_ModelWidget::onDependentParamsChanged() {
}

void WT_ModelWidget::onShowPointsToggled(bool checked) {
    MouseZoom* plot = ui->chartWidget->getPlot();
    for(int i = 0; i < plot->graphCount(); ++i) {
        if (checked) plot->graph(i)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
        else plot->graph(i)->setScatterStyle(QCPScatterStyle::ssNone);
    }
    plot->replot();
}

void WT_ModelWidget::onCalculateClicked() {
    ui->calculateButton->setEnabled(false);
    ui->calculateButton->setText("计算中...");
    QCoreApplication::processEvents();
    runCalculation();
    ui->calculateButton->setEnabled(true);
    ui->calculateButton->setText("开始计算");
}

void WT_ModelWidget::runCalculation() {
    MouseZoom* plot = ui->chartWidget->getPlot();
    plot->clearGraphs();

    // 收集界面输入参数
    QMap<QString, QVector<double>> rawParams;

    // 基础参数
    rawParams["phi"] = parseInput(ui->phiEdit->text());
    rawParams["h"] = parseInput(ui->hEdit->text());
    rawParams["rw"] = parseInput(ui->rwEdit->text());
    rawParams["mu"] = parseInput(ui->muEdit->text());
    rawParams["B"] = parseInput(ui->BEdit->text());
    rawParams["Ct"] = parseInput(ui->CtEdit->text());
    rawParams["q"] = parseInput(ui->qEdit->text());
    rawParams["t"] = parseInput(ui->tEdit->text());

    // 模型参数
    rawParams["kf"] = parseInput(ui->kfEdit->text());
    rawParams["M12"] = parseInput(ui->kmEdit->text());
    rawParams["L"] = parseInput(ui->LEdit->text());
    rawParams["Lf"] = parseInput(ui->LfEdit->text());
    rawParams["nf"] = parseInput(ui->nfEdit->text());
    rawParams["rm"] = parseInput(ui->rmDEdit->text());
    rawParams["omega1"] = parseInput(ui->omga1Edit->text());
    rawParams["omega2"] = parseInput(ui->omga2Edit->text());
    rawParams["lambda1"] = parseInput(ui->remda1Edit->text());
    rawParams["gamaD"] = parseInput(ui->gamaDEdit->text());

    QLineEdit* editRemda2 = this->findChild<QLineEdit*>("remda2Edit");
    if(editRemda2) rawParams["lambda2"] = parseInput(editRemda2->text());
    else rawParams["lambda2"] = {1e-4};

    QLineEdit* editEta12 = this->findChild<QLineEdit*>("eta12Edit");
    if(editEta12) rawParams["eta12"] = parseInput(editEta12->text());
    else rawParams["eta12"] = {0.2};

    if (ui->reDEdit->isVisible()) rawParams["re"] = parseInput(ui->reDEdit->text());
    else rawParams["re"] = {20000.0};

    // 井储参数处理 (C -> cD 转换)
    if (ui->cDEdit->isVisible()) {
        QVector<double> C_vals = parseInput(ui->cDEdit->text());
        QVector<double> cD_vals;

        // CD = 0.159 * C / (phi * h * Ct * L^2)
        double phi = rawParams["phi"].isEmpty() ? 0.05 : rawParams["phi"].first();
        double Ct = rawParams["Ct"].isEmpty() ? 5e-4 : rawParams["Ct"].first();
        double h = rawParams["h"].isEmpty() ? 20.0 : rawParams["h"].first();
        double L = rawParams["L"].isEmpty() ? 1000.0 : rawParams["L"].first();

        double denom = phi * h * Ct * L * L;
        double factor = 0.0;
        if (denom > 1e-20) factor = 0.159 / denom;

        for(double valC : C_vals) {
            cD_vals.append(valC * factor);
        }

        rawParams["cD"] = cD_vals;
        rawParams["S"] = parseInput(ui->sEdit->text());
    } else {
        rawParams["cD"] = {0.0};
        rawParams["S"] = {0.0};
    }

    // 敏感性分析检查
    QString sensitivityKey = "";
    QVector<double> sensitivityValues;
    for(auto it = rawParams.begin(); it != rawParams.end(); ++it) {
        if(it.key() == "t") continue;
        if(it.value().size() > 1) {
            sensitivityKey = it.key();
            sensitivityValues = it.value();
            break;
        }
    }
    bool isSensitivity = !sensitivityKey.isEmpty();

    // 构建基础参数字典
    QMap<QString, double> baseParams;
    for(auto it = rawParams.begin(); it != rawParams.end(); ++it) {
        baseParams[it.key()] = it.value().isEmpty() ? 0.0 : it.value().first();
    }

    baseParams["N"] = m_highPrecision ? 10.0 : 4.0;

    // 内部计算 LfD
    if(baseParams["L"] > 1e-9) baseParams["LfD"] = baseParams["Lf"] / baseParams["L"];
    else baseParams["LfD"] = 0.0;

    // 生成时间序列
    int nPoints = ui->pointsEdit->text().toInt();
    if(nPoints < 5) nPoints = 5;
    double maxTime = baseParams.value("t", 1000.0);
    if(maxTime < 1e-3) maxTime = 1000.0;
    QVector<double> t = ModelManager::generateLogTimeSteps(nPoints, -3.0, log10(maxTime));

    int iterations = isSensitivity ? sensitivityValues.size() : 1;
    iterations = qMin(iterations, (int)m_colorList.size());

    QString resultTextHeader = QString("计算完成 (%1)\n").arg(getModelName());
    if(isSensitivity) resultTextHeader += QString("敏感性参数: %1\n").arg(sensitivityKey);

    // 循环计算
    for(int i = 0; i < iterations; ++i) {
        QMap<QString, double> currentParams = baseParams;
        double val = 0;
        if (isSensitivity) {
            val = sensitivityValues[i];
            currentParams[sensitivityKey] = val;

            if (sensitivityKey == "L" || sensitivityKey == "Lf") {
                if(currentParams["L"] > 1e-9) currentParams["LfD"] = currentParams["Lf"] / currentParams["L"];
            }
        }

        ModelCurveData res = calculateTheoreticalCurve(currentParams, t);

        res_tD = std::get<0>(res);
        res_pD = std::get<1>(res);
        res_dpD = std::get<2>(res);

        QColor curveColor = isSensitivity ? m_colorList[i] : Qt::red;
        QString legendName;
        if (isSensitivity) legendName = QString("%1 = %2").arg(sensitivityKey).arg(val);
        else legendName = "理论曲线";

        plotCurve(res, legendName, curveColor, isSensitivity);
    }

    // 更新结果
    QString resultText = resultTextHeader;
    resultText += "t(h)\t\tDp(MPa)\t\tdDp(MPa)\n";
    for(int i=0; i<res_pD.size(); ++i) {
        resultText += QString("%1\t%2\t%3\n").arg(res_tD[i],0,'e',4).arg(res_pD[i],0,'e',4).arg(res_dpD[i],0,'e',4);
    }
    ui->resultTextEdit->setText(resultText);

    ui->chartWidget->getPlot()->rescaleAxes();
    if(plot->xAxis->range().lower <= 0) plot->xAxis->setRangeLower(1e-3);
    if(plot->yAxis->range().lower <= 0) plot->yAxis->setRangeLower(1e-3);
    plot->replot();

    onShowPointsToggled(ui->checkShowPoints->isChecked());
    emit calculationCompleted(getModelName(), baseParams);
}

void WT_ModelWidget::plotCurve(const ModelCurveData& data, const QString& name, QColor color, bool isSensitivity) {
    MouseZoom* plot = ui->chartWidget->getPlot();

    const QVector<double>& t = std::get<0>(data);
    const QVector<double>& p = std::get<1>(data);
    const QVector<double>& d = std::get<2>(data);

    QCPGraph* graphP = plot->addGraph();
    graphP->setData(t, p);
    graphP->setPen(QPen(color, 2, Qt::SolidLine));

    QCPGraph* graphD = plot->addGraph();
    graphD->setData(t, d);

    if (isSensitivity) {
        graphD->setPen(QPen(color, 2, Qt::DashLine));
        graphP->setName(name);
        graphD->removeFromLegend();
    } else {
        graphP->setPen(QPen(Qt::red, 2));
        graphP->setName("压力");
        graphD->setPen(QPen(Qt::blue, 2));
        graphD->setName("压力导数");
    }
}

void WT_ModelWidget::onExportData() {
    if (res_tD.isEmpty()) return;
    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";
    QString path = QFileDialog::getSaveFileName(this, "导出CSV数据", defaultDir + "/CalculatedData.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "t,Dp,dDp\n";
        for (int i = 0; i < res_tD.size(); ++i) {
            double dp = (i < res_dpD.size()) ? res_dpD[i] : 0.0;
            out << res_tD[i] << "," << res_pD[i] << "," << dp << "\n";
        }
        f.close();
        QMessageBox::information(this, "导出成功", "数据文件已保存");
    }
}
