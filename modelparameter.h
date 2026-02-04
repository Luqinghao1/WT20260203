/*
 * 文件名: modelparameter.h
 * 文件作用: 项目参数单例类头文件
 * 功能描述:
 * 1. 管理项目核心数据（包括新增的水平井长度、裂缝条数及原有的孔隙度、粘度等）和文件路径。
 * 2. 负责 _chart.json (图表) 和 _date.json (表格) 的路径生成和存取。
 * 3. 作为全局参数中心，供新建项目和恢复默认值使用。
 */

#ifndef MODELPARAMETER_H
#define MODELPARAMETER_H

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>

class ModelParameter : public QObject
{
    Q_OBJECT

public:
    static ModelParameter* instance();

    // ========================================================================
    // 项目文件管理
    // ========================================================================

    // 加载项目文件 (.pwt)
    bool loadProject(const QString& filePath);

    // 保存基础参数到 .pwt 文件
    bool saveProject();

    // 关闭项目，清空内存数据
    void closeProject();

    QString getProjectFilePath() const { return m_projectFilePath; }
    QString getProjectPath() const { return m_projectPath; }
    bool hasLoadedProject() const { return m_hasLoaded; }

    // ========================================================================
    // 基础参数存取 (全局物理参数)
    // ========================================================================

    // [修改] 增加 L (水平井长度) 和 nf (裂缝条数) 的设置接口
    void setParameters(double phi, double h, double mu, double B, double Ct,
                       double q, double rw, double L, double nf, const QString& path);

    // Getters
    double getPhi() const { return m_phi; }
    double getH() const { return m_h; }
    double getMu() const { return m_mu; }
    double getB() const { return m_B; }
    double getCt() const { return m_Ct; }
    double getQ() const { return m_q; }
    double getRw() const { return m_rw; }

    // [新增] 获取新增参数
    double getL() const { return m_L; }   // 水平井长度
    double getNf() const { return m_nf; } // 裂缝条数

    // 保存拟合结果
    void saveFittingResult(const QJsonObject& fittingData);
    QJsonObject getFittingResult() const;

    // ========================================================================
    // 独立数据文件存取
    // ========================================================================

    // 保存绘图数据到 "_chart.json"
    void savePlottingData(const QJsonArray& plots);
    QJsonArray getPlottingData() const;

    // 保存表格数据到 "_date.json"
    void saveTableData(const QJsonArray& tableData);
    QJsonArray getTableData() const;

    // 重置所有项目数据（恢复为默认值）
    void resetAllData();

private:
    explicit ModelParameter(QObject* parent = nullptr);
    static ModelParameter* m_instance;

    bool m_hasLoaded;
    QString m_projectPath;
    QString m_projectFilePath;

    // 缓存完整的JSON对象
    QJsonObject m_fullProjectData;

    // 基础物理参数变量
    double m_phi;   // 孔隙度
    double m_h;     // 有效厚度
    double m_mu;    // 粘度
    double m_B;     // 体积系数
    double m_Ct;    // 综合压缩系数
    double m_q;     // 测试产量
    double m_rw;    // 井筒半径

    // [新增]
    double m_L;     // 水平井长度
    double m_nf;    // 裂缝条数

    // 辅助：获取附属文件的绝对路径
    QString getPlottingDataFilePath() const;
    QString getTableDataFilePath() const;
};

#endif // MODELPARAMETER_H
