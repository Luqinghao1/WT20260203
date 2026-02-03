/*
 * 文件名: modelmanager.cpp
 * 文件作用: 模型管理类实现文件
 * 功能描述:
 * 1. 核心控制器：管理系统的所有试井模型界面和计算求解器。
 * 2. 惰性加载：仅在用户切换模型时才创建对应的界面和求解器，显著降低内存占用和启动时间。
 * 3. 默认参数策略：根据模型类型（双孔/均质、边界类型、井储类型）智能生成默认参数。
 * 4. 模型切换：处理模型选择弹窗的返回结果，并在堆栈窗口中切换显示。
 *
 * 修改记录:
 * 1. [扩容] 容器大小调整为 18，支持 Model_1 至 Model_18。
 * 2. [新增] 增加模型 13-18 (内区双孔+外区均质) 的切换逻辑。
 * 3. [优化] 默认参数生成逻辑适配内区/外区不同的介质类型。
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"

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
    // 清理求解器内存
    // WT_ModelWidget 作为 QWidget 子类，会由 Qt 父对象机制自动清理
    for(auto* s : m_solvers) {
        if(s) delete s;
    }
    m_solvers.clear();
    m_modelWidgets.clear();
}

// 初始化模型体系
void ModelManager::initializeModels(QWidget* parentWidget)
{
    if (!parentWidget) return;
    createMainWidget();

    m_modelStack = new QStackedWidget(m_mainWidget);

    // [关键修改] 初始化容器大小为 18，以支持所有模型 (1-18)
    // 初始填充 nullptr，实现惰性加载
    m_modelWidgets.clear();
    m_modelWidgets.resize(18);
    m_modelWidgets.fill(nullptr);

    m_solvers.clear();
    m_solvers.resize(18);
    m_solvers.fill(nullptr);

    m_mainWidget->layout()->addWidget(m_modelStack);

    // 默认加载第一个模型，此时才会触发创建 Model_1 的界面
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

// [核心] 确保 Widget 存在的辅助函数 (惰性工厂模式)
WT_ModelWidget* ModelManager::ensureWidget(ModelType type)
{
    int index = (int)type;
    // 安全检查防止越界
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

// [核心] 确保 Solver 存在的辅助函数
ModelSolver01_06* ModelManager::ensureSolver(ModelType type)
{
    int index = (int)type;
    if (index < 0 || index >= m_solvers.size()) return nullptr;

    if (m_solvers[index] == nullptr) {
        ModelSolver01_06* solver = new ModelSolver01_06(type);
        m_solvers[index] = solver;
    }
    return m_solvers[index];
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

// 响应“选择模型”按钮点击
void ModelManager::onSelectModelClicked()
{
    ModelSelect dlg(m_mainWidget);

    // 计算当前模型的代码，以便在弹窗中回显选中状态
    // Model_1(enum 0) -> modelwidget1
    int currentId = (int)m_currentModelType + 1;
    QString currentCode = QString("modelwidget%1").arg(currentId);

    // 设置当前状态到对话框
    dlg.setCurrentModelCode(currentCode);

    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();

        // 映射代码到枚举值并切换
        // [双重孔隙 + 双重孔隙]
        if (code == "modelwidget1") switchToModel(Model_1);
        else if (code == "modelwidget2") switchToModel(Model_2);
        else if (code == "modelwidget3") switchToModel(Model_3);
        else if (code == "modelwidget4") switchToModel(Model_4);
        else if (code == "modelwidget5") switchToModel(Model_5);
        else if (code == "modelwidget6") switchToModel(Model_6);
        // [均质 + 均质]
        else if (code == "modelwidget7") switchToModel(Model_7);
        else if (code == "modelwidget8") switchToModel(Model_8);
        else if (code == "modelwidget9") switchToModel(Model_9);
        else if (code == "modelwidget10") switchToModel(Model_10);
        else if (code == "modelwidget11") switchToModel(Model_11);
        else if (code == "modelwidget12") switchToModel(Model_12);
        // [双重孔隙 + 均质]
        else if (code == "modelwidget13") switchToModel(Model_13);
        else if (code == "modelwidget14") switchToModel(Model_14);
        else if (code == "modelwidget15") switchToModel(Model_15);
        else if (code == "modelwidget16") switchToModel(Model_16);
        else if (code == "modelwidget17") switchToModel(Model_17);
        else if (code == "modelwidget18") switchToModel(Model_18);
        else {
            qDebug() << "ModelManager: 未知的模型代码: " << code;
        }
    }
}

QString ModelManager::getModelTypeName(ModelType type)
{
    return ModelSolver01_06::getModelName(type);
}

void ModelManager::onWidgetCalculationCompleted(const QString &t,
                                                const QMap<QString, double> &r) {
    emit calculationCompleted(t, r);
}

void ModelManager::setHighPrecision(bool high) {
    // 遍历所有已创建的组件设置精度
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) w->setHighPrecision(high);
    }
    for(ModelSolver01_06* s : m_solvers) {
        if(s) s->setHighPrecision(high);
    }
}

void ModelManager::updateAllModelsBasicParameters()
{
    // 调用所有界面的重置参数槽函数，刷新显示 (如 phi, h 等变更后)
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) QMetaObject::invokeMethod(w, "onResetParameters");
    }
}

// [核心逻辑] 获取模型默认参数
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

    // 导压系数比，所有复合模型通用
    p.insert("eta12", 0.2);
    p.insert("gamaD", 0.02); // 压敏系数

    // 3. 介质参数配置逻辑
    // 内区双孔: 1-6, 13-18
    bool hasInnerDual = (type <= Model_6) || (type >= Model_13 && type <= Model_18);
    // 外区双孔: 1-6
    bool hasOuterDual = (type <= Model_6);

    if (hasInnerDual) {
        p.insert("omega1", 0.4);
        p.insert("lambda1", 1e-3);
    }
    if (hasOuterDual) {
        p.insert("omega2", 0.08);
        p.insert("lambda2", 1e-4);
    }

    // 4. 井储与表皮 (偶数 ID 为 Consider 变井储)
    bool hasStorage = ((int)type % 2 == 0);
    if (hasStorage) {
        p.insert("cD", 0.01);
        p.insert("S", 0.01);
    } else {
        p.insert("cD", 0.0);
        p.insert("S", 0.0);
    }

    // 5. 边界半径 (无限大模型不需要)
    // 无限大: 1, 2, 7, 8, 13, 14
    bool isInfinite = (type == Model_1 || type == Model_2 ||
                       type == Model_7 || type == Model_8 ||
                       type == Model_13 || type == Model_14);
    if (!isInfinite) {
        p.insert("re", 20000.0);
    }

    return p;
}

ModelCurveData ModelManager::calculateTheoreticalCurve(ModelType type,
                                                       const QMap<QString, double>& params,
                                                       const QVector<double>& providedTime)
{
    ModelSolver01_06* solver = ensureSolver(type);
    if (solver) {
        return solver->calculateTheoreticalCurve(params, providedTime);
    }
    return ModelCurveData();
}

QVector<double> ModelManager::generateLogTimeSteps(int count, double startExp,
                                                   double endExp) {
    return ModelSolver01_06::generateLogTimeSteps(count, startExp, endExp);
}

void ModelManager::setObservedData(const QVector<double>& t, const QVector<double>& p,
                                   const QVector<double>& d)
{
    m_cachedObsTime = t;
    m_cachedObsPressure = p;
    m_cachedObsDerivative = d;
}

void ModelManager::getObservedData(QVector<double>& t, QVector<double>& p,
                                   QVector<double>& d) const
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
