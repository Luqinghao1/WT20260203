/*
 * 文件名: modelselect.h
 * 文件作用: 模型选择对话框头文件
 * 功能描述:
 * 1. 提供井、储层、边界等条件的组合选择界面。
 * 2. 负责将用户选择的条件映射到具体的模型代码 (modelwidget1-12)。
 * 3. 支持打开时回显当前已选择的模型状态。
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
    void onSelectionChanged();
    void onAccepted();

private:
    Ui::ModelSelect *ui;
    QString m_selectedModelCode;
    QString m_selectedModelName;

    void initOptions();

    // 内部标志，防止在代码设置下拉框时频繁触发信号
    bool m_isInitializing;
};

#endif // MODELSELECT_H
