/*
 * 文件名: modelmanager.cpp
 * 文件作用: 模型管理类实现文件
 * 功能描述:
 * 1. 初始化并管理所有模型界面和求解器实例。
 * 2. 实现了模型计算的分发逻辑，支持 Model 1-36。
 * 3. 提供了智能的默认参数生成策略，适配不同模型的物理特性。
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"
#include "modelsolver19_36.h" // 引入新求解器

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
    // 清理求解器资源
    // 组1: Model 1-18
    for(auto* s : m_solversGroup1) {
        if(s) delete s;
    }
    m_solversGroup1.clear();

    // 组2: Model 19-36
    for(auto* s : m_solversGroup2) {
        if(s) delete s;
    }
    m_solversGroup2.clear();

    // 界面 Widget 由 Qt 父子对象机制管理，只需清空容器
    m_modelWidgets.clear();
}

void ModelManager::initializeModels(QWidget* parentWidget)
{
    if (!parentWidget) return;
    createMainWidget();

    m_modelStack = new QStackedWidget(m_mainWidget);

    // [修改] 扩容容器以支持 36 个模型
    m_modelWidgets.resize(36);
    m_modelWidgets.fill(nullptr); // 初始为空，惰性加载

    // 初始化求解器容器
    m_solversGroup1.resize(18);
    m_solversGroup1.fill(nullptr);

    m_solversGroup2.resize(18);
    m_solversGroup2.fill(nullptr);

    m_mainWidget->layout()->addWidget(m_modelStack);

    // 默认加载第一个模型
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
    mainLayout->setContentsMargins(0, 0, 0, 0); // 无边距嵌入
    mainLayout->setSpacing(0);
    m_mainWidget->setLayout(mainLayout);
}

// 确保指定类型的界面 Widget 已创建 (惰性工厂模式)
WT_ModelWidget* ModelManager::ensureWidget(ModelType type)
{
    int index = (int)type;
    // 边界检查
    if (index < 0 || index >= m_modelWidgets.size()) return nullptr;

    // 如果尚未创建，则立即创建
    if (m_modelWidgets[index] == nullptr) {
        WT_ModelWidget* widget = new WT_ModelWidget(type, m_modelStack);
        m_modelWidgets[index] = widget;
        m_modelStack->addWidget(widget);

        // 连接界面内部信号到管理器
        connect(widget, &WT_ModelWidget::requestModelSelection,
                this, &ModelManager::onSelectModelClicked);
        connect(widget, &WT_ModelWidget::calculationCompleted,
                this, &ModelManager::onWidgetCalculationCompleted);
    }
    return m_modelWidgets[index];
}

// [修改] 获取组1求解器 (Model 1-18)
ModelSolver01_06* ModelManager::ensureSolverGroup1(int index)
{
    if (index < 0 || index >= m_solversGroup1.size()) return nullptr;
    if (m_solversGroup1[index] == nullptr) {
        m_solversGroup1[index] = new ModelSolver01_06((ModelSolver01_06::ModelType)index);
    }
    return m_solversGroup1[index];
}

// [新增] 获取组2求解器 (Model 19-36)
ModelSolver19_36* ModelManager::ensureSolverGroup2(int index)
{
    if (index < 0 || index >= m_solversGroup2.size()) return nullptr;
    if (m_solversGroup2[index] == nullptr) {
        // ModelSolver19_36 内部枚举从 0 开始，对应外部 ID 18
        m_solversGroup2[index] = new ModelSolver19_36((ModelSolver19_36::ModelType)index);
    }
    return m_solversGroup2[index];
}

// 切换当前显示的模型
void ModelManager::switchToModel(ModelType modelType)
{
    if (!m_modelStack) return;
    ModelType old = m_currentModelType;
    m_currentModelType = modelType;

    // 确保目标模型界面已创建
    WT_ModelWidget* w = ensureWidget(modelType);
    if (w) {
        m_modelStack->setCurrentWidget(w);
    }

    emit modelSwitched(modelType, old);
}

// [核心逻辑] 统一计算接口，根据 ID 分发给不同的求解器
ModelCurveData ModelManager::calculateTheoreticalCurve(ModelType type,
                                                       const QMap<QString, double>& params,
                                                       const QVector<double>& providedTime)
{
    int id = (int)type;

    // Group 1: Model 1-18 (ID 0-17)
    if (id >= 0 && id <= 17) {
        ModelSolver01_06* solver = ensureSolverGroup1(id);
        if (solver) return solver->calculateTheoreticalCurve(params, providedTime);
    }
    // Group 2: Model 19-36 (ID 18-35)
    else if (id >= 18 && id <= 35) {
        // 内部索引 = id - 18
        ModelSolver19_36* solver = ensureSolverGroup2(id - 18);
        if (solver) return solver->calculateTheoreticalCurve(params, providedTime);
    }

    return ModelCurveData();
}

// 获取模型名称
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

// 响应“选择模型”按钮点击
void ModelManager::onSelectModelClicked()
{
    ModelSelect dlg(m_mainWidget);

    // 计算当前模型的代码，以便在弹窗中回显选中状态
    // 代码格式: modelwidgetX, 其中 X = ID + 1
    int currentId = (int)m_currentModelType + 1;
    QString currentCode = QString("modelwidget%1").arg(currentId);
    dlg.setCurrentModelCode(currentCode);

    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();

        // 解析代码: modelwidgetX -> X
        QString numStr = code;
        numStr.remove("modelwidget");
        bool ok;
        int modelId = numStr.toInt(&ok);

        // 有效范围 1-36
        if (ok && modelId >= 1 && modelId <= 36) {
            // 切换模型 (内部 ID = X - 1)
            switchToModel((ModelType)(modelId - 1));
        } else {
            qDebug() << "ModelManager: 未知的模型代码: " << code;
        }
    }
}

// [核心逻辑] 获取模型默认参数，适配新模型
QMap<QString, double> ModelManager::getDefaultParameters(ModelType type)
{
    QMap<QString, double> p;
    ModelParameter* mp = ModelParameter::instance();

    // 1. 基础参数 (从全局单例获取)
    p.insert("phi", mp->getPhi());
    p.insert("h", mp->getH());
    p.insert("mu", mp->getMu());
    p.insert("B", mp->getB());
    p.insert("Ct", mp->getCt());
    p.insert("q", mp->getQ());
    p.insert("rw", 0.1);

    // 2. 几何与流体参数默认值
    p.insert("nf", 4.0);
    p.insert("kf", 1e-3);
    p.insert("M12", 10.0);
    p.insert("L", 1000.0);
    p.insert("Lf", 20.0);
    p.insert("LfD", 0.02); // 无因次缝长
    p.insert("rm", 1000.0);
    p.insert("eta12", 0.2); // 导压系数比
    p.insert("gamaD", 0.02); // 压敏系数

    int id = (int)type;

    // 3. 介质参数配置逻辑
    bool hasInnerParams = false; // 是否需要内区参数 (omega1, lambda1)
    bool hasOuterParams = false; // 是否需要外区参数 (omega2, lambda2)

    if (id <= 17) {
        // --- Group 1 (1-18) ---
        // 内区双孔: 1-6 (0-5), 13-18 (12-17)
        if (id <= 5 || (id >= 12 && id <= 17)) hasInnerParams = true;
        // 外区双孔: 1-6 (0-5)
        if (id <= 5) hasOuterParams = true;
    }
    else {
        // --- Group 2 (19-36) ---
        // 内区全是夹层型，都需要 w1, l1
        hasInnerParams = true;

        int subId = id - 18; // 相对索引 0-17
        // 外区类型判定:
        // 19-24 (sub 0-5): 外区夹层 -> 需要 w2, l2
        // 25-30 (sub 6-11): 外区均质 -> 不需要 w2, l2
        // 31-36 (sub 12-17): 外区双孔 -> 需要 w2, l2
        if (subId <= 5 || subId >= 12) hasOuterParams = true;
    }

    if (hasInnerParams) {
        p.insert("omega1", 0.4);
        p.insert("lambda1", 1e-3);
    }
    if (hasOuterParams) {
        p.insert("omega2", 0.08);
        p.insert("lambda2", 1e-4);
    }

    // 4. 井储与表皮 (规律: ID 为偶数时考虑井储)
    // Model 1 (id 0): 考虑; Model 2 (id 1): 不考虑
    bool hasStorage = (id % 2 == 0);
    if (hasStorage) {
        p.insert("cD", 10);
        p.insert("S", 0.01);
    } else {
        p.insert("cD", 0.0);
        p.insert("S", 0.0);
    }

    // 5. 边界半径 (无限大模型不需要)
    // 规律: 每6个一组的前2个是无限大
    // (id) % 6 == 0 或 1 -> 无限大
    int rem = id % 6;
    bool isInfinite = (rem == 0 || rem == 1);
    if (!isInfinite) {
        p.insert("re", 20000.0);
    }

    return p;
}

void ModelManager::setHighPrecision(bool high) {
    // 遍历所有组件设置精度
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) w->setHighPrecision(high);
    }
    for(ModelSolver01_06* s : m_solversGroup1) {
        if(s) s->setHighPrecision(high);
    }
    for(ModelSolver19_36* s : m_solversGroup2) {
        if(s) s->setHighPrecision(high);
    }
}

void ModelManager::updateAllModelsBasicParameters()
{
    // 调用所有界面的重置参数槽函数，刷新显示
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) QMetaObject::invokeMethod(w, "onResetParameters");
    }
}

// 缓存相关函数
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
    // 复用 Solver01-06 的静态方法
    return ModelSolver01_06::generateLogTimeSteps(count, startExp, endExp);
}
