/*
 * 文件名: modelsolver19_36.h
 * 文件作用: 压裂水平井夹层型复合模型核心计算类头文件
 * 功能描述:
 * 1. 提供 Model 19-36 (共18个) 模型的计算。
 * 2. 模型特征：内区均为“夹层型”介质，外区分别为“夹层型”、“均质”、“双重孔隙”。
 * 3. 实现了Stehfest数值反演及压力导数计算。
 * 4. 继承了径向复合模型的所有几何与边界特性。
 */

#ifndef MODELSOLVER19_36_H
#define MODELSOLVER19_36_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>
#include <QtConcurrent>

// 类型定义: <时间序列(t), 压力序列(Dp), 导数序列(Dp')>
using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelSolver19_36
{
public:
    // 模型类型枚举 (对应 Model 19 - 36)
    enum ModelType {
        // --- 夹层型 + 夹层型 (19-24) ---
        Model_19 = 0, // 变井储 + 无限大
        Model_20,     // 恒定井储 + 无限大
        Model_21,     // 变井储 + 封闭
        Model_22,     // 恒定井储 + 封闭
        Model_23,     // 变井储 + 定压
        Model_24,     // 恒定井储 + 定压

        // --- 夹层型 + 均质 (25-30) ---
        Model_25,     // 变井储 + 无限大
        Model_26,     // 恒定井储 + 无限大
        Model_27,     // 变井储 + 封闭
        Model_28,     // 恒定井储 + 封闭
        Model_29,     // 变井储 + 定压
        Model_30,     // 恒定井储 + 定压

        // --- 夹层型 + 双重孔隙 (31-36) ---
        Model_31,     // 变井储 + 无限大
        Model_32,     // 恒定井储 + 无限大
        Model_33,     // 变井储 + 封闭
        Model_34,     // 恒定井储 + 封闭
        Model_35,     // 变井储 + 定压
        Model_36      // 恒定井储 + 定压
    };

    explicit ModelSolver19_36(ModelType type);
    virtual ~ModelSolver19_36();

    // 设置高精度计算模式
    void setHighPrecision(bool high);

    // 计算理论曲线接口
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    /**
     * @brief 获取模型名称
     * @param type 模型类型
     * @param verbose 是否显示详细信息
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

    // 边界元计算函数 (复用径向复合逻辑)
    double PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD,
                         int n_seg, int n_fracs, double spacingD, ModelType type);

    // 介质函数计算辅助
    double calc_fs_dual(double u, double omega, double lambda);       // 双重孔隙 f(s)
    double calc_fs_interlayer(double u, double omega, double lambda); // 夹层型 f(s) = s * f_dual(s)

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

#endif // MODELSOLVER19_36_H
