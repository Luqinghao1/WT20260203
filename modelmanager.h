/*
 * 文件名: modelmanager.h
 * 文件作用: 模型管理类头文件
 * 功能描述:
 * 1. 作为系统的核心控制器，管理所有试井模型界面 (WT_ModelWidget) 和求解器 (ModelSolver)。
 * 2. 维护当前激活的模型状态，处理模型切换逻辑。
 * 3. 提供统一的计算接口、参数默认值获取接口。
 * 4. 管理观测数据（时间、压力、导数）的缓存与分发。
 * 5. 采用“惰性初始化”策略，仅在需要时创建具体的模型界面和求解器，提升启动速度。
 *
 * 修改记录:
 * 1. [扩充] 增加 Model_7 至 Model_12 的静态常量定义，支持均质复合模型。
 * 2. [优化] 完善了对观测数据缓存的接口定义。
 */


#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QStackedWidget>
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"

class ModelManager : public QObject
{
    Q_OBJECT

public:
    using ModelType = ModelSolver01_06::ModelType;

    static const ModelType Model_1 = ModelSolver01_06::Model_1;
    static const ModelType Model_2 = ModelSolver01_06::Model_2;
    static const ModelType Model_3 = ModelSolver01_06::Model_3;
    static const ModelType Model_4 = ModelSolver01_06::Model_4;
    static const ModelType Model_5 = ModelSolver01_06::Model_5;
    static const ModelType Model_6 = ModelSolver01_06::Model_6;
    static const ModelType Model_7 = ModelSolver01_06::Model_7;
    static const ModelType Model_8 = ModelSolver01_06::Model_8;
    static const ModelType Model_9 = ModelSolver01_06::Model_9;
    static const ModelType Model_10 = ModelSolver01_06::Model_10;
    static const ModelType Model_11 = ModelSolver01_06::Model_11;
    static const ModelType Model_12 = ModelSolver01_06::Model_12;
    // [新增]
    static const ModelType Model_13 = ModelSolver01_06::Model_13;
    static const ModelType Model_14 = ModelSolver01_06::Model_14;
    static const ModelType Model_15 = ModelSolver01_06::Model_15;
    static const ModelType Model_16 = ModelSolver01_06::Model_16;
    static const ModelType Model_17 = ModelSolver01_06::Model_17;
    static const ModelType Model_18 = ModelSolver01_06::Model_18;

    explicit ModelManager(QWidget* parent = nullptr);
    ~ModelManager();

    void initializeModels(QWidget* parentWidget);
    void switchToModel(ModelType modelType);
    static QString getModelTypeName(ModelType type);
    ModelCurveData calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());
    QMap<QString, double> getDefaultParameters(ModelType type);
    void setHighPrecision(bool high);
    void updateAllModelsBasicParameters();
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);
    void getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const;
    bool hasObservedData() const;
    void clearCache();

signals:
    void modelSwitched(ModelType newType, ModelType oldType);
    void calculationCompleted(const QString& analysisType, const QMap<QString, double>& results);

private slots:
    void onSelectModelClicked();
    void onWidgetCalculationCompleted(const QString& t, const QMap<QString, double>& r);

private:
    void createMainWidget();
    WT_ModelWidget* ensureWidget(ModelType type);
    ModelSolver01_06* ensureSolver(ModelType type);

private:
    QWidget* m_mainWidget;
    QStackedWidget* m_modelStack;
    QVector<WT_ModelWidget*> m_modelWidgets;
    QVector<ModelSolver01_06*> m_solvers;
    ModelType m_currentModelType;
    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDerivative;
};

#endif // MODELMANAGER_H
