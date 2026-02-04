/*
 * 文件名: modelparameter.cpp
 * 文件作用: 项目参数单例类实现文件
 * 功能描述:
 * 1. 实现项目数据的加载与保存，包括新增的水平井参数。
 * 2. 初始化时设置符合业务需求的默认物理参数。
 * 3. 集中管理全局参数，优化参数传递逻辑。
 */

#include "modelparameter.h"
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>

ModelParameter* ModelParameter::m_instance = nullptr;

ModelParameter::ModelParameter(QObject* parent) : QObject(parent), m_hasLoaded(false)
{
    // [修改] 初始化默认值 (按照要求 3)
    m_L = 1000.0;   // 水平井长度
    m_nf = 4.0;     // 裂缝条数
    m_q = 10.0;     // 测试产量
    m_phi = 0.05;   // 孔隙度
    m_h = 10.0;     // 有效厚度
    m_rw = 0.1;     // 井筒半径
    m_Ct = 0.05;    // 综合压缩系数 (原 5e-4 -> 0.05)
    m_mu = 5.0;     // 粘度 (原 0.5 -> 5)
    m_B = 1.2;      // 体积系数 (原 1.05 -> 1.2)
}

ModelParameter* ModelParameter::instance()
{
    if (!m_instance) m_instance = new ModelParameter();
    return m_instance;
}

void ModelParameter::setParameters(double phi, double h, double mu, double B, double Ct,
                                   double q, double rw, double L, double nf, const QString& path)
{
    m_phi = phi; m_h = h; m_mu = mu; m_B = B; m_Ct = Ct; m_q = q; m_rw = rw;
    m_L = L; m_nf = nf; // [新增]

    m_projectFilePath = path;

    QFileInfo fi(path);
    m_projectPath = fi.isFile() ? fi.absolutePath() : path;
    m_hasLoaded = true;

    if (m_fullProjectData.isEmpty()) {
        QJsonObject reservoir;
        reservoir["porosity"] = m_phi;
        reservoir["thickness"] = m_h;
        reservoir["wellRadius"] = m_rw;
        reservoir["productionRate"] = m_q;
        reservoir["horizLength"] = m_L; // [新增]
        reservoir["fracCount"] = m_nf;  // [新增]

        QJsonObject pvt;
        pvt["viscosity"] = m_mu;
        pvt["volumeFactor"] = m_B;
        pvt["compressibility"] = m_Ct;

        m_fullProjectData["reservoir"] = reservoir;
        m_fullProjectData["pvt"] = pvt;
    }
}

QString ModelParameter::getPlottingDataFilePath() const
{
    if (m_projectFilePath.isEmpty()) return QString();
    QFileInfo fi(m_projectFilePath);
    QString baseName = fi.completeBaseName();
    return fi.absolutePath() + "/" + baseName + "_chart.json";
}

QString ModelParameter::getTableDataFilePath() const
{
    if (m_projectFilePath.isEmpty()) return QString();
    QFileInfo fi(m_projectFilePath);
    QString baseName = fi.completeBaseName();
    return fi.absolutePath() + "/" + baseName + "_date.json";
}

bool ModelParameter::loadProject(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开项目文件:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;

    m_fullProjectData = doc.object();

    // 解析物理参数
    if (m_fullProjectData.contains("reservoir")) {
        QJsonObject res = m_fullProjectData["reservoir"].toObject();
        // 使用要求的默认值作为 fallback
        m_q = res["productionRate"].toDouble(10.0);
        m_phi = res["porosity"].toDouble(0.05);
        m_h = res["thickness"].toDouble(10.0);
        m_rw = res["wellRadius"].toDouble(0.1);
        m_L = res["horizLength"].toDouble(1000.0); // [新增]
        m_nf = res["fracCount"].toDouble(4.0);     // [新增]
    }
    if (m_fullProjectData.contains("pvt")) {
        QJsonObject pvt = m_fullProjectData["pvt"].toObject();
        m_Ct = pvt["compressibility"].toDouble(0.05);
        m_mu = pvt["viscosity"].toDouble(5.0);
        m_B = pvt["volumeFactor"].toDouble(1.2);
    }

    m_projectFilePath = filePath;
    m_projectPath = QFileInfo(filePath).absolutePath();
    m_hasLoaded = true;

    // 加载图表数据
    QString chartPath = getPlottingDataFilePath();
    QFile chartFile(chartPath);
    if (chartFile.exists() && chartFile.open(QIODevice::ReadOnly)) {
        QJsonDocument d = QJsonDocument::fromJson(chartFile.readAll());
        if (!d.isNull() && d.isObject()) {
            QJsonObject obj = d.object();
            if (obj.contains("plotting_data")) {
                m_fullProjectData["plotting_data"] = obj["plotting_data"];
            }
        }
        chartFile.close();
    }

    // 加载表格数据
    QString datePath = getTableDataFilePath();
    QFile dateFile(datePath);
    if (dateFile.exists() && dateFile.open(QIODevice::ReadOnly)) {
        QJsonDocument d = QJsonDocument::fromJson(dateFile.readAll());
        if (!d.isNull() && d.isObject()) {
            QJsonObject obj = d.object();
            if (obj.contains("table_data")) {
                m_fullProjectData["table_data"] = obj["table_data"];
            }
        }
        dateFile.close();
    } else {
        m_fullProjectData.remove("table_data");
    }

    return true;
}

bool ModelParameter::saveProject()
{
    if (!m_hasLoaded || m_projectFilePath.isEmpty()) return false;

    QJsonObject reservoir;
    if(m_fullProjectData.contains("reservoir")) reservoir = m_fullProjectData["reservoir"].toObject();
    reservoir["porosity"] = m_phi;
    reservoir["thickness"] = m_h;
    reservoir["wellRadius"] = m_rw;
    reservoir["productionRate"] = m_q;
    reservoir["horizLength"] = m_L; // [新增]
    reservoir["fracCount"] = m_nf;  // [新增]
    m_fullProjectData["reservoir"] = reservoir;

    QJsonObject pvt;
    if(m_fullProjectData.contains("pvt")) pvt = m_fullProjectData["pvt"].toObject();
    pvt["viscosity"] = m_mu;
    pvt["volumeFactor"] = m_B;
    pvt["compressibility"] = m_Ct;
    m_fullProjectData["pvt"] = pvt;

    QJsonObject dataToWrite = m_fullProjectData;
    dataToWrite.remove("plotting_data");
    dataToWrite.remove("table_data");

    QFile file(m_projectFilePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(dataToWrite).toJson());
    file.close();

    return true;
}

void ModelParameter::closeProject()
{
    resetAllData();
}

void ModelParameter::saveFittingResult(const QJsonObject& fittingData)
{
    if (m_projectFilePath.isEmpty()) return;
    m_fullProjectData["fitting"] = fittingData;

    QFile file(m_projectFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject dataToWrite = m_fullProjectData;
        dataToWrite.remove("plotting_data");
        dataToWrite.remove("table_data");
        file.write(QJsonDocument(dataToWrite).toJson());
        file.close();
    }
}

QJsonObject ModelParameter::getFittingResult() const
{
    return m_fullProjectData.value("fitting").toObject();
}

void ModelParameter::savePlottingData(const QJsonArray& plots)
{
    if (m_projectFilePath.isEmpty()) return;
    m_fullProjectData["plotting_data"] = plots;
    QString dataFilePath = getPlottingDataFilePath();
    QJsonObject dataObj;
    dataObj["plotting_data"] = plots;
    QFile file(dataFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(dataObj).toJson());
        file.close();
    }
}

QJsonArray ModelParameter::getPlottingData() const
{
    return m_fullProjectData.value("plotting_data").toArray();
}

void ModelParameter::saveTableData(const QJsonArray& tableData)
{
    if (m_projectFilePath.isEmpty()) return;
    m_fullProjectData["table_data"] = tableData;
    QString dataFilePath = getTableDataFilePath();
    QJsonObject dataObj;
    dataObj["table_data"] = tableData;
    QFile file(dataFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(dataObj).toJson());
        file.close();
    }
}

// [修改] 重置所有参数为指定的默认值
void ModelParameter::resetAllData()
{
    m_L = 1000.0;
    m_nf = 4.0;
    m_q = 10.0;
    m_phi = 0.05;
    m_h = 10.0;
    m_rw = 0.1;
    m_Ct = 0.05;
    m_mu = 5.0;
    m_B = 1.2;

    m_hasLoaded = false;
    m_projectPath.clear();
    m_projectFilePath.clear();
    m_fullProjectData = QJsonObject();

    qDebug() << "ModelParameter: 全局参数已重置为默认值 (q=10, L=1000, etc.)。";
}

QJsonArray ModelParameter::getTableData() const
{
    return m_fullProjectData.value("table_data").toArray();
}
