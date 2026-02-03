/*
 * 文件名: modelmanager.h
 * 文件作用: 模型管理类头文件
 * 功能描述:
 * 1. 系统核心控制器：统一管理所有试井模型界面 (WT_ModelWidget) 和求解器。
 * 2. 模型定义：定义了 Model_1 到 Model_36 共36种模型的唯一标识。
 * 3. 资源管理：采用惰性初始化策略管理两组求解器 (ModelSolver01_06 和 ModelSolver19_36)。
 * 4. 接口封装：提供统一的理论曲线计算、默认参数获取、观测数据缓存接口。
 */

#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QStackedWidget>
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"
#include "modelsolver19_36.h" // [新增] 引入夹层型模型求解器头文件

class ModelManager : public QObject
{
    Q_OBJECT

public:
    // [修改] 使用 int 作为通用的模型类型标识，以支持扩展到 36 个模型
    // 0-17 对应 ModelSolver01_06, 18-35 对应 ModelSolver19_36
    using ModelType = int;

    // --- 现有模型 1-18 (对应 ID 0-17) ---
    static const int Model_1 = 0;
    static const int Model_2 = 1;
    static const int Model_3 = 2;
    static const int Model_4 = 3;
    static const int Model_5 = 4;
    static const int Model_6 = 5;
    static const int Model_7 = 6;
    static const int Model_8 = 7;
    static const int Model_9 = 8;
    static const int Model_10 = 9;
    static const int Model_11 = 10;
    static const int Model_12 = 11;
    static const int Model_13 = 12;
    static const int Model_14 = 13;
    static const int Model_15 = 14;
    static const int Model_16 = 15;
    static const int Model_17 = 16;
    static const int Model_18 = 17;

    // --- [新增] 新增模型 19-36 (对应 ID 18-35) ---
    static const int Model_19 = 18;
    static const int Model_20 = 19;
    static const int Model_21 = 20;
    static const int Model_22 = 21;
    static const int Model_23 = 22;
    static const int Model_24 = 23;
    static const int Model_25 = 24;
    static const int Model_26 = 25;
    static const int Model_27 = 26;
    static const int Model_28 = 27;
    static const int Model_29 = 28;
    static const int Model_30 = 29;
    static const int Model_31 = 30;
    static const int Model_32 = 31;
    static const int Model_33 = 32;
    static const int Model_34 = 33;
    static const int Model_35 = 34;
    static const int Model_36 = 35;

    explicit ModelManager(QWidget* parent = nullptr);
    ~ModelManager();

    // 初始化模型界面和堆栈窗口
    void initializeModels(QWidget* parentWidget);

    // 切换当前显示的模型
    void switchToModel(ModelType modelType);

    // 获取模型名称 (静态方法)
    static QString getModelTypeName(ModelType type);

    // [核心接口] 计算理论曲线 (内部自动分发给对应的求解器)
    ModelCurveData calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取指定模型的默认参数配置
    QMap<QString, double> getDefaultParameters(ModelType type);

    // 设置全局高精度模式
    void setHighPrecision(bool high);

    // 更新所有模型的显示参数 (例如当全局单位或物理属性改变时)
    void updateAllModelsBasicParameters();

    // 生成对数时间步长 (辅助工具)
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

    // --- 观测数据缓存管理 ---
    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);
    void getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const;
    bool hasObservedData() const;
    void clearCache();

signals:
    // 模型切换信号
    void modelSwitched(ModelType newType, ModelType oldType);
    // 计算完成信号
    void calculationCompleted(const QString& analysisType, const QMap<QString, double>& results);

private slots:
    // 响应界面上的"选择模型"按钮点击
    void onSelectModelClicked();
    // 响应单个 Widget 计算完成
    void onWidgetCalculationCompleted(const QString& t, const QMap<QString, double>& r);

private:
    // 创建主容器 Widget
    void createMainWidget();

    // 确保指定类型的界面 Widget 已创建 (惰性加载)
    WT_ModelWidget* ensureWidget(ModelType type);

    // [修改] 内部辅助函数：获取第一组求解器 (Model 1-18)
    ModelSolver01_06* ensureSolverGroup1(int index);

    // [新增] 内部辅助函数：获取第二组求解器 (Model 19-36)
    ModelSolver19_36* ensureSolverGroup2(int index);

private:
    QWidget* m_mainWidget;            // 主容器
    QStackedWidget* m_modelStack;     // 堆栈窗口，用于切换模型界面

    QVector<WT_ModelWidget*> m_modelWidgets; // 存储所有模型界面的指针容器

    // [修改] 分组存储求解器实例
    QVector<ModelSolver01_06*> m_solversGroup1; // 对应 ID 0-17
    QVector<ModelSolver19_36*> m_solversGroup2; // 对应 ID 18-35

    ModelType m_currentModelType;     // 当前激活的模型类型

    // 观测数据缓存
    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDerivative;
};

#endif // MODELMANAGER_H
