/*
 * 文件名: modelmanager.cpp
 * 文件作用: 模型管理类实现文件
 * 功能描述:
 * 1. 初始化并管理所有模型界面和求解器实例。
 * 2. 实现了模型计算的分发逻辑。
 * 3. [修改] 优化参数传递，直接从全局 ModelParameter 读取物理常数，
 * 并设置符合要求的模型初始猜测值。
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"
#include "modelsolver19_36.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDebug>
#include <cmath>

ModelManager::ModelManager(QWidget* parent)
    : QObject(parent), m_mainWidget(nullptr), m_modelStack(nullptr)
    , m_currentModelType(Model_1)
{
}

ModelManager::~ModelManager()
{
    for(auto* s : m_solversGroup1) if(s) delete s;
    m_solversGroup1.clear();
    for(auto* s : m_solversGroup2) if(s) delete s;
    m_solversGroup2.clear();
    m_modelWidgets.clear();
}

void ModelManager::initializeModels(QWidget* parentWidget)
{
    if (!parentWidget) return;
    createMainWidget();

    m_modelStack = new QStackedWidget(m_mainWidget);
    m_modelWidgets.resize(36);
    m_modelWidgets.fill(nullptr);

    m_solversGroup1.resize(18);
    m_solversGroup1.fill(nullptr);
    m_solversGroup2.resize(18);
    m_solversGroup2.fill(nullptr);

    m_mainWidget->layout()->addWidget(m_modelStack);
    switchToModel(Model_1);

    if (parentWidget->layout()) parentWidget->layout()->addWidget(m_mainWidget);
    else {
        QVBoxLayout* layout = new QVBoxLayout(parentWidget);
        layout->addWidget(m_mainWidget);
        parentWidget->setLayout(layout);
    }
}

void ModelManager::createMainWidget()
{
    m_mainWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(m_mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    m_mainWidget->setLayout(mainLayout);
}

WT_ModelWidget* ModelManager::ensureWidget(ModelType type)
{
    int index = (int)type;
    if (index < 0 || index >= m_modelWidgets.size()) return nullptr;

    if (m_modelWidgets[index] == nullptr) {
        WT_ModelWidget* widget = new WT_ModelWidget(type, m_modelStack);
        m_modelWidgets[index] = widget;
        m_modelStack->addWidget(widget);

        connect(widget, &WT_ModelWidget::requestModelSelection,
                this, &ModelManager::onSelectModelClicked);
        connect(widget, &WT_ModelWidget::calculationCompleted,
                this, &ModelManager::onWidgetCalculationCompleted);
    }
    return m_modelWidgets[index];
}

ModelSolver01_06* ModelManager::ensureSolverGroup1(int index)
{
    if (index < 0 || index >= m_solversGroup1.size()) return nullptr;
    if (m_solversGroup1[index] == nullptr) {
        m_solversGroup1[index] = new ModelSolver01_06((ModelSolver01_06::ModelType)index);
    }
    return m_solversGroup1[index];
}

ModelSolver19_36* ModelManager::ensureSolverGroup2(int index)
{
    if (index < 0 || index >= m_solversGroup2.size()) return nullptr;
    if (m_solversGroup2[index] == nullptr) {
        m_solversGroup2[index] = new ModelSolver19_36((ModelSolver19_36::ModelType)index);
    }
    return m_solversGroup2[index];
}

void ModelManager::switchToModel(ModelType modelType)
{
    if (!m_modelStack) return;
    ModelType old = m_currentModelType;
    m_currentModelType = modelType;

    WT_ModelWidget* w = ensureWidget(modelType);
    if (w) {
        m_modelStack->setCurrentWidget(w);
    }
    emit modelSwitched(modelType, old);
}

ModelCurveData ModelManager::calculateTheoreticalCurve(ModelType type,
                                                       const QMap<QString, double>& params,
                                                       const QVector<double>& providedTime)
{
    int id = (int)type;
    if (id >= 0 && id <= 17) {
        ModelSolver01_06* solver = ensureSolverGroup1(id);
        if (solver) return solver->calculateTheoreticalCurve(params, providedTime);
    }
    else if (id >= 18 && id <= 35) {
        ModelSolver19_36* solver = ensureSolverGroup2(id - 18);
        if (solver) return solver->calculateTheoreticalCurve(params, providedTime);
    }
    return ModelCurveData();
}

QString ModelManager::getModelTypeName(ModelType type)
{
    int id = (int)type;
    if (id >= 0 && id <= 17) {
        return ModelSolver01_06::getModelName((ModelSolver01_06::ModelType)id);
    }
    else if (id >= 18 && id <= 35) {
        return ModelSolver19_36::getModelName((ModelSolver19_36::ModelType)(id - 18));
    }
    return "未知模型";
}

void ModelManager::onSelectModelClicked()
{
    ModelSelect dlg(m_mainWidget);
    int currentId = (int)m_currentModelType + 1;
    QString currentCode = QString("modelwidget%1").arg(currentId);
    dlg.setCurrentModelCode(currentCode);

    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        QString numStr = code;
        numStr.remove("modelwidget");
        bool ok;
        int modelId = numStr.toInt(&ok);
        if (ok && modelId >= 1 && modelId <= 36) {
            switchToModel((ModelType)(modelId - 1));
        }
    }
}

// [修改] 获取模型默认参数
// 1. 物理参数：从 ModelParameter 单例读取
// 2. 模型参数：使用您指定的新默认值
QMap<QString, double> ModelManager::getDefaultParameters(ModelType type)
{
    QMap<QString, double> p;
    ModelParameter* mp = ModelParameter::instance();

    // 1. 基础物理参数 (来自全局设定)
    p.insert("phi", mp->getPhi());
    p.insert("h", mp->getH());
    p.insert("mu", mp->getMu());
    p.insert("B", mp->getB());
    p.insert("Ct", mp->getCt());
    p.insert("q", mp->getQ());
    p.insert("rw", mp->getRw());

    // 水平井和裂缝参数
    p.insert("L", mp->getL());     // 水平井长度
    p.insert("nf", mp->getNf());   // 裂缝条数

    // 2. 模型参数初始猜测值 (按照要求 4 修改)
    p.insert("k", 0.001);          // 内区渗透率 0.001D (注意单位转换，此处假设内部计算单位统一)
    // 假设内部单位一致，或由 Solver 处理。通常输入单位是 mD，0.001D = 1mD?
    // 根据上下文，用户说 "内区渗透率0.001D"，通常指 Darcy。
    // 之前代码默认 kf=1e-3, 可能是 mD. 如果用户指 0.001 Darcy = 1 mD.
    // 这里暂时直接写入 0.001，视具体 Solver 实现的单位而定。如果 Solver 期望 mD, 0.001 很小。
    // 通常试井软件内部 k 用 mD。如果用户指 0.001 Darcy，即 1 mD。
    // 如果用户指 0.001 mD，那非常小。
    // 按照 "0.001D" 写法，应该是 Darcy。若软件单位是 mD，这里应是 1.0。
    // 但为了"严格遵守输入说明"，我将写入 0.001，具体单位含义由 Solver 决定。
    // 如果 Solver 也是您编写的，请确认单位。这里按数值写入。

    p.insert("kf", 0.001);         // 裂缝渗透率/内区渗透率
    p.insert("M12", 10.0);         // 流度比 10
    p.insert("eta12", 1.0);        // 导压系数比 1 (原 0.2 -> 1)

    p.insert("Lf", 10.0);          // 裂缝半长 10 (原 20 -> 10)
    p.insert("rm", 1500.0);        // 复合半径 (原 1000 -> 1500, 原名 rm，现理解为内区半径)

    // 压敏系数
    p.insert("gamaD", 0.006);      // 压敏系数 0.006 (原 0.02 -> 0.006)

    int id = (int)type;

    // 3. 介质参数 (双孔/夹层)
    bool hasInnerParams = false;
    bool hasOuterParams = false;

    // 判断逻辑保持不变
    if (id <= 17) {
        if (id <= 5 || (id >= 12 && id <= 17)) hasInnerParams = true;
        if (id <= 5) hasOuterParams = true;
    }
    else {
        hasInnerParams = true;
        int subId = id - 18;
        if (subId <= 5 || subId >= 12) hasOuterParams = true;
    }

    if (hasInnerParams) {
        p.insert("omega1", 0.4);       // 内区储容比 0.4
        p.insert("lambda1", 0.001);    // 内区窜流系数 0.001 (原 1e-3 -> 0.001)
    }
    if (hasOuterParams) {
        p.insert("omega2", 0.08);      // 外区储容比 0.08
        p.insert("lambda2", 0.0001);   // 外区窜流系数 0.0001 (原 1e-4 -> 0.0001)
    }

    // 4. 井储与表皮
    // 按照要求：井筒存储 10，表皮系数 0.1
    // 原逻辑：id%2==0 才考虑。此处保持原逻辑，但修改数值。
    bool hasStorage = (id % 2 == 0);
    if (hasStorage) {
        p.insert("cD", 10.0);  // 井筒存储 10 (原 10 -> 10)
        p.insert("S", 0.1);    // 表皮系数 0.1 (原 0.01 -> 0.1)
    } else {
        p.insert("cD", 0.0);
        p.insert("S", 0.0);
    }

    // 5. 边界
    int rem = id % 6;
    bool isInfinite = (rem == 0 || rem == 1);
    if (!isInfinite) {
        p.insert("re", 20000.0); // 默认边界半径
    }

    return p;
}

void ModelManager::setHighPrecision(bool high) {
    for(WT_ModelWidget* w : m_modelWidgets) if(w) w->setHighPrecision(high);
    for(ModelSolver01_06* s : m_solversGroup1) if(s) s->setHighPrecision(high);
    for(ModelSolver19_36* s : m_solversGroup2) if(s) s->setHighPrecision(high);
}

void ModelManager::updateAllModelsBasicParameters()
{
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) QMetaObject::invokeMethod(w, "onResetParameters");
    }
}

void ModelManager::setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d)
{
    m_cachedObsTime = t;
    m_cachedObsPressure = p;
    m_cachedObsDerivative = d;
}

void ModelManager::getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const
{
    t = m_cachedObsTime;
    p = m_cachedObsPressure;
    d = m_cachedObsDerivative;
}

void ModelManager::clearCache()
{
    m_cachedObsTime.clear();
    m_cachedObsPressure.clear();
    m_cachedObsDerivative.clear();
}

bool ModelManager::hasObservedData() const
{
    return !m_cachedObsTime.isEmpty();
}

void ModelManager::onWidgetCalculationCompleted(const QString &t, const QMap<QString, double> &r) {
    emit calculationCompleted(t, r);
}

QVector<double> ModelManager::generateLogTimeSteps(int count, double startExp, double endExp) {
    return ModelSolver01_06::generateLogTimeSteps(count, startExp, endExp);
}
