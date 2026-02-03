/*
 * 文件名: modelselect.h
 * 文件作用: 模型选择对话框头文件
 * 功能描述:
 * 1. 提供井、储层、边界等条件的组合选择界面。
 * 2. 负责将用户选择的条件映射到具体的模型代码 (modelwidget1-36 等)。
 * 3. 支持打开时回显当前已选择的模型状态。
 * 4. [本次修改] 增加了根据储层模型动态更新内外区模型选项的槽函数。
 */

#ifndef MODELSELECT_H
#define MODELSELECT_H

#include <QDialog>

namespace Ui {
class ModelSelect;
}

class ModelSelect : public QDialog
{
    Q_OBJECT

public:
    explicit ModelSelect(QWidget *parent = nullptr);
    ~ModelSelect();

    // 获取选中的模型代码 ID (例如 "modelwidget1")
    QString getSelectedModelCode() const;
    // 获取选中的模型显示名称
    QString getSelectedModelName() const;

    // 设置当前模型代码，用于打开窗口时回显之前的选择
    void setCurrentModelCode(const QString& code);

private slots:
    // 所有的下拉框选择变化都会触发此槽函数，用于计算最终的模型代码
    void onSelectionChanged();
    // 确认按钮点击槽函数
    void onAccepted();
    // [新增] 当储层模型改变时，更新内外区模型的选项列表
    void updateInnerOuterOptions();

private:
    Ui::ModelSelect *ui;
    QString m_selectedModelCode;
    QString m_selectedModelName;

    // 初始化所有下拉框的静态选项
    void initOptions();

    // 内部标志，防止在代码设置下拉框时频繁触发信号导致逻辑错误
    bool m_isInitializing;
};

#endif // MODELSELECT_H
