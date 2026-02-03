/*
 * 文件名: modelsolver19_36.cpp
 * 文件作用: 压裂水平井夹层型复合模型核心计算类实现
 * 优化记录:
 * 1. [修复波动] 针对夹层型模型的高刚性(High Stiffness)特征，重写了 PWD_composite 中的积分逻辑。
 * 2. [自适应积分] 引入积分上限截断策略 (Cut-off)，当 gamma 很大时，只对非零区域积分，避免数值积分失效。
 * 3. [稳定性] 增加了对极小参数的保护。
 */

#include "modelsolver19_36.h"
#include "pressurederivativecalculator.h"

#include <Eigen/Dense>
#include <boost/math/special_functions/bessel.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QDebug>
#include <QtConcurrent>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Point2D { double x; double y; };

static double safe_bessel_k(int v, double x) {
    if (x < 1e-15) x = 1e-15;
    // 超过700会导致下溢为0，这是物理事实，直接返回0以避免库函数内部异常
    if (x > 700.0) return 0.0;
    try { return boost::math::cyl_bessel_k(v, x); } catch (...) { return 0.0; }
}

static double safe_bessel_i_scaled(int v, double x) {
    if (x < 0) x = -x;
    if (x > 600.0) return 1.0 / std::sqrt(2.0 * M_PI * x); // 大参数渐近近似
    try { return boost::math::cyl_bessel_i(v, x) * std::exp(-x); } catch (...) { return 0.0; }
}

ModelSolver19_36::ModelSolver19_36(ModelType type)
    : m_type(type), m_highPrecision(true), m_currentN(0) {
    // 对于刚性模型，Stehfest 系数计算不需要过高阶，10-12是合适的
    precomputeStehfestCoeffs(10);
}

ModelSolver19_36::~ModelSolver19_36() {}

void ModelSolver19_36::setHighPrecision(bool high) {
    m_highPrecision = high;
    // 即使高精度模式，对于此类模型 N=10 也比 N=18 稳定
    if (m_highPrecision && m_currentN != 10) precomputeStehfestCoeffs(10);
    else if (!m_highPrecision && m_currentN != 6) precomputeStehfestCoeffs(6);
}

// 获取模型名称
QString ModelSolver19_36::getModelName(ModelType type, bool verbose)
{
    int modelId = (int)type + 19;
    QString baseName = QString("压力水平井夹层型模型%1").arg(modelId - 18);

    if (!verbose) {
        return baseName;
    }

    bool hasStorage = ((int)type % 2 == 0);
    QString strStorage = hasStorage ? "考虑井储表皮" : "不考虑井储表皮";

    int rem = (int)type % 6;
    QString strBoundary;
    if (rem == 0 || rem == 1) strBoundary = "无限大外边界";
    else if (rem == 2 || rem == 3) strBoundary = "封闭边界";
    else strBoundary = "定压边界";

    QString strMedium;
    if (type >= Model_19 && type <= Model_24) strMedium = "夹层型+夹层型";
    else if (type >= Model_25 && type <= Model_30) strMedium = "夹层型+均质";
    else strMedium = "夹层型+双重孔隙";

    return QString("%1\n(%2、%3、%4)").arg(baseName).arg(strStorage).arg(strBoundary).arg(strMedium);
}

QVector<double> ModelSolver19_36::generateLogTimeSteps(int count, double startExp, double endExp)
{
    QVector<double> t;
    if (count <= 0) return t;
    t.reserve(count);
    for (int i = 0; i < count; ++i) {
        double exponent = startExp + (endExp - startExp) * i / (count - 1);
        t.append(pow(10.0, exponent));
    }
    return t;
}

ModelCurveData ModelSolver19_36::calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    QVector<double> tPoints = providedTime;
    if (tPoints.isEmpty()) tPoints = generateLogTimeSteps(100, -3.0, 3.0);

    double phi = params.value("phi", 0.05);
    double mu = params.value("mu", 0.5);
    double B = params.value("B", 1.05);
    double Ct = params.value("Ct", 5e-4);
    double q = params.value("q", 5.0);
    double h = params.value("h", 20.0);
    double kf = params.value("kf", 1e-3);
    double L = params.value("L", 1000.0);

    if (L < 1e-9) L = 1000.0;
    if (phi < 1e-12 || mu < 1e-12 || Ct < 1e-12 || kf < 1e-12) {
        return std::make_tuple(tPoints, QVector<double>(tPoints.size(), 0.0), QVector<double>(tPoints.size(), 0.0));
    }

    double td_coeff = 14.4 * kf / (phi * mu * Ct * pow(L, 2));
    QVector<double> tD_vec;
    tD_vec.reserve(tPoints.size());
    for(double t : tPoints) tD_vec.append(td_coeff * t);

    QMap<QString, double> calcParams = params;
    // 强制检查 N 值，对于夹层模型，过大的 N (如12以上) 极易导致震荡
    int N = (int)calcParams.value("N", 10);
    if (N < 4 || N > 12 || N % 2 != 0) N = 10;
    calcParams["N"] = N;
    precomputeStehfestCoeffs(N);

    if (!calcParams.contains("nf") || calcParams["nf"] < 1) calcParams["nf"] = 1;
    if (!calcParams.contains("n_seg")) calcParams["n_seg"] = 5;
    if (calcParams["n_seg"] < 1) calcParams["n_seg"] = 1;

    QVector<double> PD_vec, Deriv_vec;
    auto func = std::bind(&ModelSolver19_36::flaplace_composite, this, std::placeholders::_1, std::placeholders::_2);
    calculatePDandDeriv(tD_vec, calcParams, func, PD_vec, Deriv_vec);

    double p_coeff = 1.842e-3 * q * mu * B / (kf * h);
    QVector<double> finalP(tPoints.size()), finalDP(tPoints.size());
    for(int i=0; i<tPoints.size(); ++i) {
        finalP[i] = p_coeff * PD_vec[i];
        finalDP[i] = p_coeff * Deriv_vec[i];
    }
    return std::make_tuple(tPoints, finalP, finalDP);
}

void ModelSolver19_36::calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                                           std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                                           QVector<double>& outPD, QVector<double>& outDeriv)
{
    int numPoints = tD.size();
    outPD.resize(numPoints);
    outDeriv.resize(numPoints);

    int N = m_currentN;
    double ln2 = 0.6931471805599453;
    double gamaD = params.value("gamaD", 0.0);

    QVector<int> indexes(numPoints);
    std::iota(indexes.begin(), indexes.end(), 0);

    auto calculateSinglePoint = [&](int k) {
        double t = tD[k];
        if (t <= 1e-10) { outPD[k] = 0.0; return; }

        double pd_val = 0.0;
        for (int m = 1; m <= N; ++m) {
            double z = m * ln2 / t;
            double pf = laplaceFunc(z, params);
            if (std::isnan(pf) || std::isinf(pf)) pf = 0.0;
            pd_val += getStehfestCoeff(m, N) * pf;
        }

        double pd_real = pd_val * ln2 / t;
        if (std::abs(gamaD) > 1e-9) {
            double arg = 1.0 - gamaD * pd_real;
            if (arg > 1e-12) pd_real = -1.0 / gamaD * std::log(arg);
        }
        outPD[k] = pd_real;
    };

    QtConcurrent::blockingMap(indexes, calculateSinglePoint);

    if (numPoints > 2) {
        outDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(tD, outPD, 0.1);
    } else {
        outDeriv.fill(0.0);
    }
}

double ModelSolver19_36::calc_fs_dual(double u, double omega, double lambda) {
    double one_minus = 1.0 - omega;
    double den = one_minus * u + lambda;
    if (std::abs(den) < 1e-20) return 0.0;
    return (omega * one_minus * u + lambda) / den;
}

double ModelSolver19_36::calc_fs_interlayer(double u, double omega, double lambda) {
    return u * calc_fs_dual(u, omega, lambda);
}

double ModelSolver19_36::flaplace_composite(double z, const QMap<QString, double>& p) {
    double M12 = p.contains("M12") ? p.value("M12") : 1.0;
    double L = p.value("L", 1000.0);
    double Lf = p.value("Lf", 100.0);
    double rm = p.value("rm", 500.0);
    double re = p.value("re", 20000.0);
    double LfD = (L > 1e-9) ? Lf / L : 0.1;
    double rmD = (L > 1e-9) ? rm / L : 0.5;
    double reD = (L > 1e-9) ? re / L : 20.0;

    double eta12 = p.value("eta", 0.2);
    if (p.contains("eta12")) eta12 = p.value("eta12");

    int n_fracs = (int)p.value("nf", 1);
    int n_seg = (int)p.value("n_seg", 10);
    if (n_fracs < 1) n_fracs = 1;
    if (n_seg < 1) n_seg = 1;

    double spacingD = (n_fracs > 1) ? 0.9 / (double)(n_fracs - 1) : 0.0;

    double fs1 = 1.0;
    double fs2 = 1.0;

    double omga1 = p.value("omega1", 0.4);
    double remda1 = p.contains("lambda1") ? p.value("lambda1") : p.value("remda1", 1e-3);
    fs1 = calc_fs_interlayer(z, omga1, remda1);

    double z_outer = eta12 * z;

    if (m_type >= Model_19 && m_type <= Model_24) {
        double omga2 = p.value("omega2", 0.08);
        double remda2 = p.contains("lambda2") ? p.value("lambda2") : p.value("remda2", 1e-4);
        fs2 = eta12 * calc_fs_interlayer(z_outer, omga2, remda2);
    }
    else if (m_type >= Model_25 && m_type <= Model_30) {
        fs2 = eta12;
    }
    else if (m_type >= Model_31 && m_type <= Model_36) {
        double omga2 = p.value("omega2", 0.08);
        double remda2 = p.contains("lambda2") ? p.value("lambda2") : p.value("remda2", 1e-4);
        fs2 = eta12 * calc_fs_dual(z_outer, omga2, remda2);
    }

    // 调用通用边界元求解
    double pf = PWD_composite(z, fs1, fs2, M12, LfD, rmD, reD, n_seg, n_fracs, spacingD, m_type);

    bool hasStorage = ((int)m_type % 2 == 0);
    if (hasStorage) {
        double CD = p.value("cD", 0.0);
        double S = p.value("S", 0.0);
        if (CD > 1e-12 || std::abs(S) > 1e-12) {
            double num = z * pf + S;
            double den = z + CD * z * z * num;
            if (std::abs(den) > 1e-100) pf = num / den;
        }
    }
    return pf;
}

// [核心优化] 边界元求解函数，增加积分稳定性处理
double ModelSolver19_36::PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD,
                                       int n_seg, int n_fracs, double spacingD, ModelType type) {
    int total_segments = n_fracs * n_seg;
    double segLen = 2.0 * LfD / n_seg;
    QVector<Point2D> segmentCenters;
    segmentCenters.reserve(total_segments);
    double startX = -(n_fracs - 1) * spacingD / 2.0;

    for (int k = 0; k < n_fracs; ++k) {
        double currentX = startX + k * spacingD;
        for (int i = 0; i < n_seg; ++i) {
            double currentY = -LfD + (i + 0.5) * segLen;
            segmentCenters.append({currentX, currentY});
        }
    }

    double gama1 = sqrt(z * fs1);
    double gama2 = sqrt(z * fs2);
    double arg_g1_rm = gama1 * rmD;
    double arg_g2_rm = gama2 * rmD;

    // Bessel函数预计算
    // 注意：对于夹层型，z 很大时 gamma 也很大，导致 arg_g1_rm 可能 > 700
    // 此时 K0, K1 为 0，I0, I1 极大。必须依靠 scaled I 进行计算。

    double k0_g2_rm = safe_bessel_k(0, arg_g2_rm);
    double k1_g2_rm = safe_bessel_k(1, arg_g2_rm);
    double k0_g1_rm = safe_bessel_k(0, arg_g1_rm);
    double k1_g1_rm = safe_bessel_k(1, arg_g1_rm);

    double term_mAB_i0 = 0.0;
    double term_mAB_i1 = 0.0;

    int rem = (int)type % 6;
    bool isInfinite = (rem == 0 || rem == 1);
    bool isClosed = (rem == 2 || rem == 3);
    bool isConstP = (rem == 4 || rem == 5);

    if (!isInfinite && reD > 1e-5) {
        double arg_re = gama2 * reD;
        // 使用 scaled I 避免溢出
        double i0_re_s = safe_bessel_i_scaled(0, arg_re);
        double i1_re_s = safe_bessel_i_scaled(1, arg_re);
        double k1_re = safe_bessel_k(1, arg_re);
        double k0_re = safe_bessel_k(0, arg_re);

        double i0_g2_rm_s = safe_bessel_i_scaled(0, arg_g2_rm);
        double i1_g2_rm_s = safe_bessel_i_scaled(1, arg_g2_rm);

        // 指数修正项
        double exp_factor = 0.0;
        if ((arg_g2_rm - arg_re) > -700.0) exp_factor = std::exp(arg_g2_rm - arg_re);

        if (isClosed && i1_re_s > 1e-100) {
            term_mAB_i0 = (k1_re / i1_re_s) * i0_g2_rm_s * exp_factor;
            term_mAB_i1 = (k1_re / i1_re_s) * i1_g2_rm_s * exp_factor;
        } else if (isConstP && i0_re_s > 1e-100) {
            term_mAB_i0 = -(k0_re / i0_re_s) * i0_g2_rm_s * exp_factor;
            term_mAB_i1 = -(k0_re / i0_re_s) * i1_g2_rm_s * exp_factor;
        }
    }

    double term1 = term_mAB_i0 + k0_g2_rm;
    double term2 = term_mAB_i1 - k1_g2_rm;

    double Acup = M12 * gama1 * k1_g1_rm * term1 + gama2 * k0_g1_rm * term2;
    double i1_g1_rm_s = safe_bessel_i_scaled(1, arg_g1_rm);
    double i0_g1_rm_s = safe_bessel_i_scaled(0, arg_g1_rm);

    double Acdown_scaled = M12 * gama1 * i1_g1_rm_s * term1 - gama2 * i0_g1_rm_s * term2;
    if (std::abs(Acdown_scaled) < 1e-100) Acdown_scaled = (Acdown_scaled >= 0 ? 1e-100 : -1e-100);

    // Ac_prefactor 本质上包含了 exp(-arg_g1_rm) 的因子
    double Ac_prefactor = Acup / Acdown_scaled;

    int size = total_segments + 1;
    Eigen::MatrixXd A_mat(size, size);
    Eigen::VectorXd b_vec(size);
    b_vec.setZero();
    b_vec(total_segments) = 1.0;

    double halfLen = segLen / 2.0;

    // [稳定性优化] 积分截断半径
    // 当 gama1 很大时，K0(gama1 * x) 衰减极快。
    // 如果积分上限 halfLen 远大于有效半径，高斯积分点会分布在 0 值区域，导致计算误差。
    // K0(15) ~ 3e-7, K0(20) ~ 2e-9. 截断阈值设为 15.0/gama1 是安全的。
    double effectiveRadius = 15.0 / (gama1 > 1e-10 ? gama1 : 1e-10);
    double integrationLimit = (halfLen < effectiveRadius) ? halfLen : effectiveRadius;

    for (int i = 0; i < total_segments; ++i) {
        for (int j = i; j < total_segments; ++j) {
            Point2D pi = segmentCenters[i];
            Point2D pj = segmentCenters[j];
            double dx_sq = (pi.x - pj.x) * (pi.x - pj.x);

            // 如果两个段距离太远以至于相互作用忽略不计，直接设为0，减少噪声
            // 距离 > effectiveRadius 时，K0 ~ 0
            if (i != j) {
                double dist_centers = std::sqrt(dx_sq + (pi.y - pj.y)*(pi.y - pj.y));
                if (dist_centers > effectiveRadius + halfLen) { // +halfLen 是保守估计
                    A_mat(i, j) = 0.0;
                    A_mat(j, i) = 0.0;
                    continue;
                }
            }

            auto integrand = [&](double a) -> double {
                double dy = pi.y - (pj.y + a);
                double dist_val = std::sqrt(dx_sq + dy * dy);
                double arg_dist = gama1 * dist_val;

                double term2_val = 0.0;
                double exponent = arg_dist - arg_g1_rm;
                // 只有当指数不太小时才计算，防止下溢导致的精度噪声
                if (exponent > -700.0) {
                    term2_val = Ac_prefactor * safe_bessel_i_scaled(0, arg_dist) * std::exp(exponent);
                }

                // safe_bessel_k 内部已有 <700 判断
                return safe_bessel_k(0, arg_dist) + term2_val;
            };

            double val = 0.0;
            // 使用优化后的积分上限 integrationLimit
            if (i == j) {
                // 自感应：重点在 0 附近的奇异性，必须精细积分
                // 使用 integrationLimit 而非 halfLen
                val = 2.0 * adaptiveGauss(integrand, 0.0, integrationLimit, 1e-7, 0, 10);
            }
            else {
                // 互感应
                // 如果是相邻段，仍然需要较高精度
                // 如果距离较远但仍在有效半径内，降低精度要求
                if (std::abs(pi.x - pj.x) < 1e-9 && std::abs(pi.y - pj.y) < segLen * 1.5) {
                    val = adaptiveGauss(integrand, -halfLen, halfLen, 1e-6, 0, 6);
                } else {
                    val = adaptiveGauss(integrand, -halfLen, halfLen, 1e-5, 0, 4);
                }
            }

            double element = val / (M12 * 2.0 * LfD);
            A_mat(i, j) = element;
            if (i != j) A_mat(j, i) = element;
        }
    }

    for (int i = 0; i < total_segments; ++i) {
        A_mat(i, total_segments) = -1.0;
        A_mat(total_segments, i) = z;
    }
    A_mat(total_segments, total_segments) = 0.0;

    // 使用 LU 分解求解线性方程组
    Eigen::VectorXd x_sol = A_mat.partialPivLu().solve(b_vec);
    return x_sol(total_segments);
}

double ModelSolver19_36::scaled_besseli(int v, double x) { return safe_bessel_i_scaled(v, x); }
double ModelSolver19_36::gauss15(std::function<double(double)> f, double a, double b) {
    static const double X[] = { 0.0, 0.20119409, 0.39415135, 0.57097217, 0.72441773, 0.84820658, 0.93729853, 0.98799252 };
    static const double W[] = { 0.20257824, 0.19843149, 0.18616100, 0.16626921, 0.13957068, 0.10715922, 0.07036605, 0.03075324 };
    double h = 0.5 * (b - a);
    double c = 0.5 * (a + b);
    double s = W[0] * f(c);
    for (int i = 1; i < 8; ++i) {
        double dx = h * X[i];
        s += W[i] * (f(c - dx) + f(c + dx));
    }
    return s * h;
}
double ModelSolver19_36::adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth) {
    double c = (a + b) / 2.0;
    double v1 = gauss15(f, a, b);
    double v2 = gauss15(f, a, c) + gauss15(f, c, b);
    if (depth >= maxDepth || std::abs(v1 - v2) < eps * (std::abs(v2) + 1.0)) return v2;
    return adaptiveGauss(f, a, c, eps/2, depth+1, maxDepth) + adaptiveGauss(f, c, b, eps/2, depth+1, maxDepth);
}
void ModelSolver19_36::precomputeStehfestCoeffs(int N) {
    if (m_currentN == N && !m_stehfestCoeffs.isEmpty()) return;
    m_currentN = N; m_stehfestCoeffs.resize(N + 1);
    for (int i = 1; i <= N; ++i) {
        double s = 0.0;
        int k1 = (i + 1) / 2;
        int k2 = std::min(i, N / 2);
        for (int k = k1; k <= k2; ++k) {
            double num = std::pow((double)k, N / 2.0) * factorial(2 * k);
            double den = factorial(N / 2 - k) * factorial(k) * factorial(k - 1) * factorial(i - k) * factorial(2 * k - i);
            if (den != 0) s += num / den;
        }
        double sign = ((i + N / 2) % 2 == 0) ? 1.0 : -1.0;
        m_stehfestCoeffs[i] = sign * s;
    }
}
double ModelSolver19_36::getStehfestCoeff(int i, int N) {
    if (m_currentN != N) return 0.0;
    if (i < 1 || i > N) return 0.0;
    return m_stehfestCoeffs[i];
}
double ModelSolver19_36::factorial(int n) {
    if(n <= 1) return 1.0;
    double r = 1.0;
    for(int i = 2; i <= n; ++i) r *= i;
    return r;
}
