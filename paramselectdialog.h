/*
 * 文件名: paramselectdialog.h
 * 文件作用: 拟合参数选择对话框头文件
 * 修改记录:
 * 1. 构造函数增加 modelType 和 fittingTime 参数。
 * 2. 新增 onResetParams 和 onAutoLimits 槽函数。
 * 3. 新增 getFittingTime 方法。
 */

#ifndef PARAMSELECTDIALOG_H
#define PARAMSELECTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QEvent>
#include "fittingparameterchart.h"

namespace Ui {
class ParamSelectDialog;
}

class ParamSelectDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数：增加模型类型和当前拟合时间
    explicit ParamSelectDialog(const QList<FitParameter>& params,
                               ModelManager::ModelType modelType,
                               double fitTime,
                               QWidget *parent = nullptr);
    ~ParamSelectDialog();

    // 获取用户修改后的参数列表
    QList<FitParameter> getUpdatedParams() const;

    // 获取修改后的拟合时间
    double getFittingTime() const;

protected:
    // 事件过滤器，用于屏蔽输入框的鼠标滚轮事件
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::ParamSelectDialog *ui;

    // 暂存的参数列表副本
    QList<FitParameter> m_params;

    // 当前模型类型（用于重置默认值）
    ModelManager::ModelType m_modelType;

    // 初始化表格视图
    void initTable();
    // 收集数据
    void collectData();

private slots:
    void onConfirm();
    void onCancel();

    // [新增] 重置为默认参数
    void onResetParams();
    // [新增] 自动更新参数上下限
    void onAutoLimits();
};

#endif // PARAMSELECTDIALOG_H
