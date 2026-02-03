/*
 * 文件名: fittingparameterchart.cpp
 * 文件作用: 拟合参数图表管理类实现文件
 * 修改记录:
 * 1. 提取 generateDefaultParams 和 adjustLimits 为静态方法，供弹窗共用。
 * 2. 优化了参数生成逻辑结构。
 */

#include "fittingparameterchart.h"
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDebug>
#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

// 引入 ModelSolver 头文件以访问枚举值
#include "modelsolver01-06.h"

FittingParameterChart::FittingParameterChart(QTableWidget *parentTable, QObject *parent)
    : QObject(parent), m_table(parentTable), m_modelManager(nullptr)
{
    // 初始化滚轮防抖定时器
    m_wheelTimer = new QTimer(this);
    m_wheelTimer->setSingleShot(true);
    m_wheelTimer->setInterval(200); // 200ms 延迟触发重绘
    connect(m_wheelTimer, &QTimer::timeout, this, &FittingParameterChart::onWheelDebounceTimeout);

    if(m_table) {
        // 配置表格列头
        QStringList headers;
        headers << "序号" << "参数名称" << "数值" << "单位";
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);

        // 设置表头样式
        m_table->horizontalHeader()->setStyleSheet(
            "QHeaderView::section { background-color: #E0E0E0; color: black; font-weight: bold; border: 1px solid #A0A0A0; }"
            );

        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_table->horizontalHeader()->setStretchLastSection(true);

        m_table->setColumnWidth(0, 40);  // 序号列
        m_table->setColumnWidth(1, 160); // 名称列
        m_table->setColumnWidth(2, 80);  // 数值列

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setAlternatingRowColors(false);
        m_table->verticalHeader()->setVisible(false);

        // 安装事件过滤器用于滚轮支持
        m_table->viewport()->installEventFilter(this);
        connect(m_table, &QTableWidget::itemChanged, this, &FittingParameterChart::onTableItemChanged);
    }
}

bool FittingParameterChart::eventFilter(QObject *watched, QEvent *event)
{
    // 处理滚轮事件：当鼠标悬停在数值列时，可以通过滚轮调节数值
    if (watched == m_table->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        QPoint pos = wheelEvent->position().toPoint();
        QTableWidgetItem *item = m_table->itemAt(pos);

        // 仅在第2列(数值列)生效
        if (item && item->column() == 2) {
            int row = item->row();
            QTableWidgetItem *keyItem = m_table->item(row, 1);
            if (!keyItem) return false;
            QString paramName = keyItem->data(Qt::UserRole).toString();

            // LfD 是计算出来的，不允许直接修改
            if (paramName == "LfD") return true;

            FitParameter *targetParam = nullptr;
            for (auto &p : m_params) {
                if (p.name == paramName) {
                    targetParam = &p;
                    break;
                }
            }

            if (targetParam) {
                QString currentText = item->text();
                // 如果是敏感性分析的逗号分隔值，禁止滚轮修改
                if (currentText.contains(',') || currentText.contains(QChar(0xFF0C))) return false;

                bool ok;
                double currentVal = currentText.toDouble(&ok);
                if (ok) {
                    int steps = wheelEvent->angleDelta().y() / 120; // 滚轮刻度
                    double newVal = currentVal + steps * targetParam->step;

                    // 范围限制
                    if (targetParam->max > targetParam->min) {
                        if (newVal < targetParam->min) newVal = targetParam->min;
                        if (newVal > targetParam->max) newVal = targetParam->max;
                    }
                    item->setText(QString::number(newVal, 'g', 6));
                    targetParam->value = newVal;
                    m_wheelTimer->start(); // 重置定时器
                    return true; // 拦截事件，防止表格滚动
                }
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void FittingParameterChart::onWheelDebounceTimeout()
{
    emit parameterChangedByWheel();
}

void FittingParameterChart::onTableItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 2) return;
    int row = item->row();
    QTableWidgetItem *keyItem = m_table->item(row, 1);
    if (!keyItem) return;

    QString changedKey = keyItem->data(Qt::UserRole).toString();

    // 联动逻辑：当 L 或 Lf 改变时，自动更新 LfD (无因次缝长)
    if (changedKey == "L" || changedKey == "Lf") {
        double valL = 0.0;
        double valLf = 0.0;
        QTableWidgetItem* itemLfD = nullptr;

        // 遍历查找相关项
        for(int i = 0; i < m_table->rowCount(); ++i) {
            QTableWidgetItem* k = m_table->item(i, 1);
            QTableWidgetItem* v = m_table->item(i, 2);
            if(k && v) {
                QString key = k->data(Qt::UserRole).toString();
                if (key == "L") valL = v->text().toDouble();
                else if (key == "Lf") valLf = v->text().toDouble();
                else if (key == "LfD") itemLfD = v;
            }
        }

        if (valL > 1e-9) {
            if(itemLfD) {
                double newLfD = valLf / valL;
                m_table->blockSignals(true);
                itemLfD->setText(QString::number(newLfD, 'g', 6));
                m_table->blockSignals(false);
                // 同步更新内部数据
                for(auto& p : m_params) { if(p.name == "LfD") { p.value = newLfD; break; } }
            }
            // 触发防抖更新图表
            if(!m_wheelTimer->isActive()) m_wheelTimer->start();
        }
    }
}

void FittingParameterChart::setModelManager(ModelManager *m)
{
    m_modelManager = m;
}

// [静态方法] 获取指定模型的默认参数键列表
QStringList FittingParameterChart::getDefaultFitKeys(ModelManager::ModelType type)
{
    QStringList keys;
    // 基础核心参数
    keys << "kf" << "M12" << "L" << "Lf" << "nf" << "rm";

    // 判断是否为均质模型 (7-12)
    bool isHomogeneous = (type >= ModelSolver01_06::Model_7 && type <= ModelSolver01_06::Model_12);

    if (!isHomogeneous) {
        keys << "omega1" << "omega2" << "lambda1" << "lambda2";
    }

    // 所有复合模型都包含 eta12
    keys << "eta12";

    // 判断是否考虑井储 (偶数ID为Consider)
    bool hasStorage = ((int)type % 2 == 0);
    if (hasStorage) {
        keys << "C" << "S";
    }

    // 判断是否显示边界半径 re
    // 无限大边界: 1, 2, 7, 8 -> 不显示 re
    bool isInfinite = (type == ModelSolver01_06::Model_1 || type == ModelSolver01_06::Model_2 ||
                       type == ModelSolver01_06::Model_7 || type == ModelSolver01_06::Model_8);
    if (!isInfinite) {
        keys << "re";
    }

    return keys;
}

// [静态方法] 生成默认参数列表
QList<FitParameter> FittingParameterChart::generateDefaultParams(ModelManager::ModelType type)
{
    QList<FitParameter> params;

    // 辅助lambda
    auto addParam = [&](QString name, double val, bool isFitDefault) {
        FitParameter p;
        p.name = name;
        p.value = val;
        p.isFit = isFitDefault;
        p.isVisible = true;
        p.min = 0; p.max = 0; p.step = 0; // 后续自动计算

        QString symbol, uniSym, unit;
        getParamDisplayInfo(p.name, p.displayName, symbol, uniSym, unit);
        params.append(p);
    };

    // 1. 基础物理参数
    addParam("phi", 0.05, false);
    addParam("h", 20.0, false);
    addParam("rw", 0.1, false);
    addParam("mu", 0.5, false);
    addParam("B", 1.05, false);
    addParam("Ct", 5e-4, false);
    addParam("q", 5.0, false);

    // 2. 试井解释核心参数
    addParam("kf", 1e-2, true);
    addParam("M12", 10.0, true);
    addParam("eta12", 0.2, true);

    double valL = 1000.0;
    addParam("L", valL, true);
    addParam("Lf", 20.0, true);
    addParam("nf", 4.0, true);
    addParam("rm", valL, true); // 默认复合半径等于井长

    // 3. 边界半径
    bool isInfinite = (type == ModelSolver01_06::Model_1 || type == ModelSolver01_06::Model_2 ||
                       type == ModelSolver01_06::Model_7 || type == ModelSolver01_06::Model_8);
    if(!isInfinite) {
        addParam("re", 20000.0, true);
    }

    // 4. 双重介质参数 (1-6)
    bool isHomogeneous = (type >= ModelSolver01_06::Model_7 && type <= ModelSolver01_06::Model_12);
    if (!isHomogeneous) {
        addParam("omega1", 0.4, true);
        addParam("omega2", 0.08, true);
        addParam("lambda1", 1e-3, true);
        addParam("lambda2", 1e-4, true);
    }

    // 5. 井储与表皮 (偶数ID)
    bool hasStorage = ((int)type % 2 == 0);
    if(hasStorage) {
        addParam("C", 0.01, true);
        addParam("S", 0.01, true);
    }

    // 6. 其他参数
    addParam("gamaD", 0.02, false);

    // 7. 辅助显示参数
    {
        FitParameter p;
        p.name = "LfD";
        p.displayName = "无因次缝长";
        p.value = 0.02;
        p.isFit = false;
        p.isVisible = true;
        p.step = 0.0;
        params.append(p);
    }

    return params;
}

// [静态方法] 计算参数上下限
void FittingParameterChart::adjustLimits(QList<FitParameter>& params)
{
    for(auto& p : params) {
        if(p.name == "LfD") continue; // 辅助参数不设限

        double val = p.value;

        // 通用范围计算：当前值的 0.1倍 到 10倍
        if (std::abs(val) > 1e-15) {
            if (val > 0) {
                p.min = val * 0.1;
                p.max = val * 10.0;
            } else {
                p.min = val * 10.0;
                p.max = val * 0.1;
            }
        } else {
            p.min = 0.0;
            p.max = 1.0;
        }

        // 特殊参数微调
        if (p.name == "phi" || p.name.startsWith("omega") || p.name == "eta12") {
            if (p.max > 1.0) p.max = 1.0; // 分数不能大于1
            if (p.min < 0.0) p.min = 0.0001;
        }

        // 确保物理参数非负
        if (p.name == "kf" || p.name == "M12" || p.name == "L" || p.name == "Lf" ||
            p.name == "rm" || p.name == "re" || p.name.startsWith("lambda") ||
            p.name == "h" || p.name == "rw" || p.name == "mu" || p.name == "B" ||
            p.name == "Ct" || p.name == "C" || p.name == "q") {
            if (p.min <= 0.0) p.min = std::abs(val) * 0.01;
            if (p.min <= 1e-20) p.min = 1e-6;
        }

        // 裂缝条数为整数
        if (p.name == "nf") {
            if (p.min < 1.0) p.min = 1.0;
            p.min = std::ceil(p.min);
            p.max = std::floor(p.max);
        }

        // 表皮系数可以是负数
        if (p.name == "S" && std::abs(val) < 1e-9) {
            p.min = -5.0; p.max = 20.0;
        }

        // 计算步长：范围的 1/20，取整到 nice number
        double range = p.max - p.min;
        if (range > 1e-20) {
            double rawStep = range / 20.0;
            double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
            double normalized = rawStep / magnitude;
            double roundedNorm = std::round(normalized * 10.0) / 10.0;
            if (roundedNorm < 0.1) roundedNorm = 0.1;
            p.step = roundedNorm * magnitude;
        } else {
            p.step = 0.1;
        }

        if (p.name == "nf") p.step = 1.0;
    }
}

// [核心方法] 重置参数列表
void FittingParameterChart::resetParams(ModelManager::ModelType type, bool preserveStates)
{
    // 1. 备份状态
    QMap<QString, QPair<bool, bool>> stateBackup;
    if (preserveStates) {
        for(const auto& p : m_params) {
            stateBackup[p.name] = qMakePair(p.isFit, p.isVisible);
        }
    }

    // 2. 生成默认参数
    m_params = generateDefaultParams(type);

    // 3. 恢复状态
    if (preserveStates) {
        for(auto& p : m_params) {
            if(stateBackup.contains(p.name)) {
                p.isFit = stateBackup[p.name].first;
                p.isVisible = stateBackup[p.name].second;
            }
        }
    }

    // 4. 自动计算上下限并刷新
    autoAdjustLimits();
    refreshParamTable();
}

void FittingParameterChart::autoAdjustLimits()
{
    // 调用静态方法计算
    adjustLimits(m_params);
}

QList<FitParameter> FittingParameterChart::getParameters() const { return m_params; }
void FittingParameterChart::setParameters(const QList<FitParameter> &params) { m_params = params; refreshParamTable(); }

void FittingParameterChart::switchModel(ModelManager::ModelType newType)
{
    // 切换模型时，先暂存当前各参数的值
    QMap<QString, double> oldValues;
    for(const auto& p : m_params) oldValues.insert(p.name, p.value);

    // 重置为新模型的默认列表 (不保留Fit状态)
    resetParams(newType, false);

    // 恢复旧值
    for(auto& p : m_params) {
        if(oldValues.contains(p.name)) p.value = oldValues[p.name];
    }

    // 重新计算上下限
    autoAdjustLimits();

    // 特殊逻辑：校验复合半径和 LfD
    double currentL = 1000.0;
    for(const auto& p : m_params) if(p.name == "L") currentL = p.value;

    for(auto& p : m_params) {
        if(p.name == "rm") {
            if(p.min < currentL) p.min = currentL;
            if(p.value < p.min) p.value = p.min;
        }
        if(p.name == "LfD") {
            double currentLf = 20.0;
            for(const auto& pp : m_params) if(pp.name == "Lf") currentLf = pp.value;
            if(currentL > 1e-9) p.value = currentLf / currentL;
        }
    }

    refreshParamTable();
}

void FittingParameterChart::updateParamsFromTable()
{
    if(!m_table) return;
    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* itemKey = m_table->item(i, 1);
        if(!itemKey) continue;
        QString key = itemKey->data(Qt::UserRole).toString();
        QTableWidgetItem* itemVal = m_table->item(i, 2);

        QString text = itemVal->text();
        double val = 0.0;

        // 支持逗号分隔的敏感性分析输入
        if (text.contains(',') || text.contains(QChar(0xFF0C))) {
            QString firstPart = text.split(QRegularExpression("[,，]"), Qt::SkipEmptyParts).first();
            val = firstPart.toDouble();
        } else {
            val = text.toDouble();
        }

        for(auto& p : m_params) {
            if(p.name == key) { p.value = val; break; }
        }
    }
}

QMap<QString, QString> FittingParameterChart::getRawParamTexts() const
{
    QMap<QString, QString> rawTexts;
    if(!m_table) return rawTexts;
    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* itemKey = m_table->item(i, 1);
        QTableWidgetItem* itemVal = m_table->item(i, 2);
        if (itemKey && itemVal) {
            QString key = itemKey->data(Qt::UserRole).toString();
            rawTexts.insert(key, itemVal->text());
        }
    }
    return rawTexts;
}

void FittingParameterChart::refreshParamTable()
{
    if(!m_table) return;
    m_table->blockSignals(true);
    m_table->setRowCount(0);
    int serialNo = 1;

    // 分两批显示：先显示勾选拟合的，再显示未勾选的
    for(const auto& p : m_params) {
        if(p.isVisible && p.isFit) addRowToTable(p, serialNo, true);
    }
    for(const auto& p : m_params) {
        if(p.isVisible && !p.isFit) addRowToTable(p, serialNo, false);
    }

    m_table->blockSignals(false);
}

void FittingParameterChart::addRowToTable(const FitParameter& p, int& serialNo, bool highlight)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);

    // 高亮色背景
    QColor bgColor = highlight ? QColor(255, 255, 224) : Qt::white;
    if (p.name == "LfD") bgColor = QColor(245, 245, 245);

    // 1. 序号
    QTableWidgetItem* numItem = new QTableWidgetItem(QString::number(serialNo++));
    numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
    numItem->setTextAlignment(Qt::AlignCenter);
    numItem->setBackground(bgColor);
    m_table->setItem(row, 0, numItem);

    // 2. 参数名 (显示名 + 英文名)
    QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);
    QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    nameItem->setData(Qt::UserRole, p.name);
    nameItem->setBackground(bgColor);
    if(highlight) { QFont f = nameItem->font(); f.setBold(true); nameItem->setFont(f); }
    m_table->setItem(row, 1, nameItem);

    // 3. 数值
    QTableWidgetItem* valItem = new QTableWidgetItem(QString::number(p.value, 'g', 6));
    valItem->setBackground(bgColor);
    if(highlight) { QFont f = valItem->font(); f.setBold(true); valItem->setFont(f); }
    if (p.name == "LfD") {
        valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
        valItem->setForeground(QBrush(Qt::darkGray));
    }
    m_table->setItem(row, 2, valItem);

    // 4. 单位
    QString dummy, symbol, uniSym, unit;
    getParamDisplayInfo(p.name, dummy, symbol, uniSym, unit);
    if(unit == "无因次" || unit == "小数") unit = "-";
    QTableWidgetItem* unitItem = new QTableWidgetItem(unit);
    unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
    unitItem->setBackground(bgColor);
    m_table->setItem(row, 3, unitItem);
}

void FittingParameterChart::getParamDisplayInfo(const QString &name, QString &chName, QString &symbol, QString &uniSym, QString &unit)
{
    // 集中管理参数的显示信息
    if(name == "kf")          { chName = "内区渗透率";     unit = "D"; }
    else if(name == "M12")    { chName = "流度比";         unit = "无因次"; }
    else if(name == "L")      { chName = "水平井长";       unit = "m"; }
    else if(name == "Lf")     { chName = "裂缝半长";       unit = "m"; }
    else if(name == "rm")     { chName = "复合半径";       unit = "m"; }
    else if(name == "omega1") { chName = "内区储容比";     unit = "无因次"; }
    else if(name == "omega2") { chName = "外区储容比";     unit = "无因次"; }
    else if(name == "lambda1"){ chName = "内区窜流系数";   unit = "无因次"; }
    else if(name == "lambda2"){ chName = "外区窜流系数";   unit = "无因次"; }
    else if(name == "re")     { chName = "外区半径";       unit = "m"; }
    else if(name == "eta12")  { chName = "导压系数比";     unit = "无因次"; }
    else if(name == "nf")     { chName = "裂缝条数";       unit = "条"; }
    else if(name == "h")      { chName = "有效厚度";       unit = "m"; }
    else if(name == "rw")     { chName = "井筒半径";       unit = "m"; }
    else if(name == "phi")    { chName = "孔隙度";         unit = "小数"; }
    else if(name == "mu")     { chName = "流体粘度";       unit = "mPa·s"; }
    else if(name == "B")      { chName = "体积系数";       unit = "无因次"; }
    else if(name == "Ct")     { chName = "综合压缩系数";   unit = "MPa⁻¹"; }
    else if(name == "q")      { chName = "测试产量";       unit = "m³/d"; }
    else if(name == "C")      { chName = "井筒储存系数";   unit = "m³/MPa"; }
    else if(name == "cD")     { chName = "无因次井储";     unit = "无因次"; }
    else if(name == "S")      { chName = "表皮系数";       unit = "无因次"; }
    else if(name == "gamaD")  { chName = "压敏系数";       unit = "无因次"; }
    else if(name == "LfD")    { chName = "无因次缝长";     unit = "无因次"; }
    else { chName = name; unit = ""; }

    symbol = name;
    uniSym = name;
}
