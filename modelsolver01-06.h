/*
 * 文件名: modelsolver01-06.h
 * 文件作用: 压裂水平井复合页岩油模型核心计算类头文件
 * 功能描述:
 * 1. 提供18种不同边界、井储及储层介质组合的压裂水平井试井模型计算。
 * - 模型 1-6:  内区双重孔隙 + 外区双重孔隙
 * - 模型 7-12: 内区均质 + 外区均质
 * - 模型 13-18: 内区双重孔隙 + 外区均质
 * 2. 支持多条横向裂缝(Transverse Fractures)及其沿缝长的离散化计算。
 * 3. 实现了Stehfest数值反演算法及压力导数计算。
 * 4. 支持并行计算加速。
 */

#ifndef MODELSOLVER01_06_H
#define MODELSOLVER01_06_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>
#include <QtConcurrent>

// 类型定义: <时间序列(t), 压力序列(Dp), 导数序列(Dp')>
using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelSolver01_06
{
public:
    // 模型类型枚举
    enum ModelType {
        // --- 双重孔隙 + 双重孔隙 (1-6) ---
        Model_1 = 0, // 变井储 + 无限大
        Model_2,     // 恒定井储 + 无限大
        Model_3,     // 变井储 + 封闭
        Model_4,     // 恒定井储 + 封闭
        Model_5,     // 变井储 + 定压
        Model_6,     // 恒定井储 + 定压

        // --- 均质 + 均质 (7-12) ---
        Model_7,     // 变井储 + 无限大
        Model_8,     // 恒定井储 + 无限大
        Model_9,     // 变井储 + 封闭
        Model_10,    // 恒定井储 + 封闭
        Model_11,    // 变井储 + 定压
        Model_12,    // 恒定井储 + 定压

        // --- 双重孔隙 + 均质 (13-18) ---
        Model_13,    // 变井储 + 无限大
        Model_14,    // 恒定井储 + 无限大
        Model_15,    // 变井储 + 封闭
        Model_16,    // 恒定井储 + 封闭
        Model_17,    // 变井储 + 定压
        Model_18     // 恒定井储 + 定压
    };

    explicit ModelSolver01_06(ModelType type);
    virtual ~ModelSolver01_06();

    // 设置高精度计算模式
    void setHighPrecision(bool high);

    // 计算理论曲线接口
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    /**
     * @brief 获取模型名称
     * @param type 模型类型
     * @param verbose 是否显示详细信息（括号内的条件）。默认为 true。
     * 界面按钮显示时建议设为 false。
     */
    static QString getModelName(ModelType type, bool verbose = true);

    // 生成时间步
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

private:
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    // Laplace空间解主函数
    double flaplace_composite(double z, const QMap<QString, double>& p);

    // 边界元计算函数
    double PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD,
                         int n_seg, int n_fracs, double spacingD, ModelType type);

    // 数学辅助函数
    double scaled_besseli(int v, double x);
    double gauss15(std::function<double(double)> f, double a, double b);
    double adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth);

    // Stehfest算法辅助
    double getStehfestCoeff(int i, int N);
    void precomputeStehfestCoeffs(int N);
    double factorial(int n);

private:
    ModelType m_type;
    bool m_highPrecision;
    QVector<double> m_stehfestCoeffs;
    int m_currentN;
};

#endif // MODELSOLVER01_06_H
