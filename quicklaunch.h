#ifndef QUICKLAUNCH_H
#define QUICKLAUNCH_H

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
// 确保头部包含了所需的动画宏和头文件
#include <QPropertyAnimation>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif
#include <QMessageBox>

// ================= 设置对话框类 =================
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(int pages, bool autoStart, int opacity, QWidget *parent = nullptr);

    int getPageCount() const;
    bool getAutoStart() const;
    int getOpacity() const;

private:
    QSpinBox *pageSpinBox;
    QCheckBox *autoStartCheckBox;
    QSlider *opacitySlider;
};

// ================= 主窗口类 =================
class QuickLaunch : public QWidget
{
    Q_OBJECT

public:
    QuickLaunch(QWidget *parent = nullptr);
    ~QuickLaunch();

protected:
    // 鼠标事件 (仅保留拖动功能)
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    // ===== 新增：鼠标释放与进出事件 =====
    void mouseReleaseEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    // ===================================

    // 拖放事件
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void openSettings();
    void toggleExpand(); // 切换展开/收起
    void showContextMenu(const QPoint &pos); // 显示右键菜单
    void deleteSelectedItem(); // 删除选中的图标
    // ================= 新增：统一的显示/隐藏控制逻辑 =================
    void toggleVisibility();
    // ===== 新增：显示关于对话框的槽函数 =====
    void showAboutDialog();

private:
    void setupUI();
    void setupTrayIcon();
    QListWidget* createGridListWidget();
    void addAppToList(QListWidget* listWidget, const QString& filePath);

    void loadSettings();
    void applyConfig(int pages, bool autoStart, int opacity);
    void setWindowsAutoStart(bool enable);
    void saveCurrentTabPaths(); // 保存当前标签页的快捷方式
    // ===== 新增：动画函数和贴边状态 =====
    void animateWindowGeometry(const QRect &endValue);
    bool isDockedTop = false; // 记录是否处于顶部贴边状态

    QTabWidget *tabWidget;
    QPushButton *expandBtn;
    QSystemTrayIcon *trayIcon;
    QPoint dragPosition;

    // 状态控制变量
    bool isExpanded = false;

    // 当前配置变量
    int m_pageCount = 2;
    bool m_autoStart = false;
    int m_opacity = 100;

    // 尺寸常量
    const int WidthCollapsed = 460;
    const int WidthExpanded = 880;
    const int WindowHeight = 110;
};

#endif // QUICKLAUNCH_H