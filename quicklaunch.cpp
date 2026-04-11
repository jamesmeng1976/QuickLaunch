#include "QuickLaunch.h"
#include <QFileInfo>
#include <QFileIconProvider>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QApplication>
#include <QStyle>
#include <QDir>
#include <QFormLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>

// ================= SettingsDialog 实现 =================
SettingsDialog::SettingsDialog(int pages, bool autoStart, int opacity, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("工具栏设置");
    setFixedSize(300, 180);

    QFormLayout *layout = new QFormLayout(this);

    pageSpinBox = new QSpinBox(this);
    pageSpinBox->setRange(1, 10);
    pageSpinBox->setValue(pages);
    layout->addRow("页面数量:", pageSpinBox);

    autoStartCheckBox = new QCheckBox("开启随系统自启动", this);
    autoStartCheckBox->setChecked(autoStart);
    layout->addRow("", autoStartCheckBox);

    opacitySlider = new QSlider(Qt::Horizontal, this);
    opacitySlider->setRange(20, 100);
    opacitySlider->setValue(opacity);
    layout->addRow("界面不透明度:", opacitySlider);

    QPushButton *okBtn = new QPushButton("确定", this);
    QPushButton *cancelBtn = new QPushButton("取消", this);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addRow(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

int SettingsDialog::getPageCount() const { return pageSpinBox->value(); }
bool SettingsDialog::getAutoStart() const { return autoStartCheckBox->isChecked(); }
int SettingsDialog::getOpacity() const { return opacitySlider->value(); }


// ================= QuickLaunch 主类实现 =================
QuickLaunch::QuickLaunch(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAcceptDrops(true);

    // ================== 核心修复：防止多屏拖拽缩水 ==================
    setFixedHeight(WindowHeight);         // 彻底锁死高度，无论系统怎么算，高度一像素都不许少
    setMinimumWidth(WidthCollapsed);      // 设定宽度的最低底线，防止宽度被压缩
    // ================================================================

    resize(WidthCollapsed, WindowHeight); // 默认收起状态

    setupUI();
    setupTrayIcon();
    loadSettings();
}
QuickLaunch::~QuickLaunch() {}

void QuickLaunch::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    tabWidget = new QTabWidget(this);
    // 【关键修改 1】：通过 QSS 的 left 属性，把所有的 Tab 标签向右平移 30 像素，腾出左上角的空间
    tabWidget->setStyleSheet("QTabWidget::pane { border: 1px solid #999999; } "
                             "QTabWidget::tab-bar { left: 30px; }");

    // 【关键修改 2】：放弃 CornerWidget，直接基于主窗口进行“绝对定位”
    QPushButton *hideBtn = new QPushButton("✖", this);
    // 强制固定在主窗口的 x=0, y=0 的绝对坐标，宽30，高24
    hideBtn->setGeometry(0, 0, 30, 24);
    hideBtn->setToolTip("隐藏到托盘");
    // 默认背景透明 (transparent)，鼠标悬浮变红
    hideBtn->setStyleSheet("QPushButton { border: none; color: #666666; font-weight: bold; background: transparent; }"
                           "QPushButton:hover { background-color: #ff4c4c; color: white; }");

    connect(hideBtn, &QPushButton::clicked, this, &QuickLaunch::hide);

    expandBtn = new QPushButton("▶", this);
    expandBtn->setFixedSize(20, WindowHeight);
    expandBtn->setStyleSheet("QPushButton { background-color: #d0d0d0; border: none; }"
                             "QPushButton:hover { background-color: #b0b0b0; }");
    connect(expandBtn, &QPushButton::clicked, this, &QuickLaunch::toggleExpand);

    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(expandBtn);

    // 【关键修改 3】：确保这个悬浮按钮层级在最上面，不会被 TabWidget 盖住
    hideBtn->raise();
}

QListWidget* QuickLaunch::createGridListWidget()
{
    QListWidget *listWidget = new QListWidget(this);
    listWidget->setViewMode(QListView::IconMode);
    listWidget->setFlow(QListView::LeftToRight); // 单行流式布局
    listWidget->setWrapping(false);              // 不换行
    listWidget->setIconSize(QSize(48, 48));

    // ================== 新增与修改区域 ==================
    // 1. 设置严格的网格大小 (85x85)
    listWidget->setGridSize(QSize(85, 85));

    // 2. 强制所有图标项的底层尺寸保持完全一致，忽略文字长短
    listWidget->setUniformItemSizes(true);

    // 3. 开启内部拖拽，并强制拖动时吸附到网格 (SnapToGrid)
    listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    listWidget->setDefaultDropAction(Qt::MoveAction);
    listWidget->setMovement(QListView::Snap); // 关键：拖动吸附网格
    // ====================================================

    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 开启右键菜单
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listWidget, &QListWidget::customContextMenuRequested, this, &QuickLaunch::showContextMenu);

    // ================== QSS 样式表修改 ==================
    // 4. 在 QListWidget::item 中增加 width 和 height 强行锁死 3D 框的尺寸
    // 网格是 85x85，减去 margin: 4px (左右共8px)，所以宽高设为 75px 最完美。
    listWidget->setStyleSheet(
        "QListWidget { background-color: #e0e0e0; border: 2px inset #ffffff; }"
        "QListWidget::item { "
        "   width: 75px; height: 75px; " /* 关键修复：锁死 3D 框体尺寸 */
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fdfdfd, stop:1 #c8c8c8); "
        "   border: 1px solid #a0a0a0; border-bottom: 2px solid #707070; border-radius: 6px; margin: 4px; color: #333333; "
        "}"
        "QListWidget::item:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #e0e0e0); }"
        "QListWidget::item:selected { background: #b1d6fd; border-bottom: 1px solid #707070; margin-top: 5px; }"
        );

    // 点击启动程序
    connect(listWidget, &QListWidget::itemClicked, this, &QuickLaunch::onItemDoubleClicked);

    // 监听位置变动并自动保存
    connect(listWidget->model(), &QAbstractItemModel::rowsMoved, this, &QuickLaunch::saveCurrentTabPaths);

    return listWidget;
}

// ================= 动画与尺寸控制 =================
void QuickLaunch::toggleExpand()
{
    isExpanded = !isExpanded;
    expandBtn->setText(isExpanded ? "◀" : "▶");
    int targetWidth = isExpanded ? WidthExpanded : WidthCollapsed;

    QPropertyAnimation *anim = new QPropertyAnimation(this, "geometry");
    anim->setDuration(300);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(geometry());
    anim->setEndValue(QRect(x(), y(), targetWidth, height()));
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ================= 系统托盘与设置菜单 =================
void QuickLaunch::setupTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip("快速启动工具栏");

    QMenu *trayMenu = new QMenu(this);

    QAction *toggleVisAction = new QAction("显示/隐藏主界面(&V)", this);
    QAction *settingsAction = new QAction("设置(&S)", this);

    // ===== 新增：“关于”菜单项 =====
    QAction *aboutAction = new QAction("关于软件(&A)", this);
    // ===============================

    QAction *quitAction = new QAction("退出(&Q)", this);

    connect(toggleVisAction, &QAction::triggered, this, &QuickLaunch::toggleVisibility);
    connect(settingsAction, &QAction::triggered, this, &QuickLaunch::openSettings);

    // ===== 新增：绑定点击事件到我们的槽函数 =====
    connect(aboutAction, &QAction::triggered, this, &QuickLaunch::showAboutDialog);
    // ============================================

    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayMenu->addAction(toggleVisAction);
    trayMenu->addSeparator();
    trayMenu->addAction(settingsAction);

    // ===== 新增：把“关于”加进菜单，通常放在退出之前 =====
    trayMenu->addAction(aboutAction);
    // ===================================================

    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);

    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &QuickLaunch::onTrayIconActivated);
    trayIcon->show();
}

// 统一的显示/隐藏处理逻辑
void QuickLaunch::toggleVisibility()
{
    if (isVisible()) {
        hide(); // 如果当前可见，则隐藏
    } else {
        showNormal();
        activateWindow(); // 确保窗口弹出时获取焦点并置顶
    }
}

// 托盘双击事件响应
void QuickLaunch::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    // 只有当双击左键时，才触发显示/隐藏
    if (reason == QSystemTrayIcon::DoubleClick) {
        toggleVisibility();
    }
}

void QuickLaunch::openSettings()
{
    SettingsDialog dlg(m_pageCount, m_autoStart, m_opacity, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyConfig(dlg.getPageCount(), dlg.getAutoStart(), dlg.getOpacity());
    }
}

// ================= 配置与注册表操作 =================
void QuickLaunch::setWindowsAutoStart(bool enable)
{
    QString appName = "MyQuickLaunch";
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enable) reg.setValue(appName, appPath);
    else reg.remove(appName);
}

void QuickLaunch::applyConfig(int pages, bool autoStart, int opacity)
{
    m_pageCount = pages;
    m_autoStart = autoStart;
    m_opacity = opacity;

    setWindowOpacity(m_opacity / 100.0);
    setWindowsAutoStart(m_autoStart);

    while (tabWidget->count() < m_pageCount) {
        tabWidget->addTab(createGridListWidget(), QString("页面 %1").arg(tabWidget->count() + 1));
    }
    while (tabWidget->count() > m_pageCount) {
        tabWidget->removeTab(tabWidget->count() - 1);
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("Settings/PageCount", m_pageCount);
    settings.setValue("Settings/AutoStart", m_autoStart);
    settings.setValue("Settings/Opacity", m_opacity);
}

void QuickLaunch::loadSettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    m_pageCount = settings.value("Settings/PageCount", 2).toInt();
    m_autoStart = settings.value("Settings/AutoStart", false).toBool();
    m_opacity = settings.value("Settings/Opacity", 100).toInt();

    applyConfig(m_pageCount, m_autoStart, m_opacity);

    for (int i = 0; i < tabWidget->count(); ++i) {
        QString key = QString("Page_%1/Paths").arg(i);
        QStringList paths = settings.value(key).toStringList();
        QListWidget *currentList = qobject_cast<QListWidget*>(tabWidget->widget(i));
        if (currentList) {
            for (const QString &path : paths) {
                addAppToList(currentList, path);
            }
        }
    }
}

void QuickLaunch::saveCurrentTabPaths()
{
    int index = tabWidget->currentIndex();
    QListWidget *currentList = qobject_cast<QListWidget*>(tabWidget->widget(index));
    if (!currentList) return;

    QStringList paths;
    for (int i = 0; i < currentList->count(); ++i) {
        paths.append(currentList->item(i)->data(Qt::UserRole).toString());
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue(QString("Page_%1/Paths").arg(index), paths);
}

// ================= 图标与启动逻辑 =================
void QuickLaunch::addAppToList(QListWidget* listWidget, const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) return;

    QFileIconProvider iconProvider;
    QIcon icon = iconProvider.icon(fileInfo);
    QString fileName = fileInfo.completeBaseName();

    QListWidgetItem *item = new QListWidgetItem(icon, fileName);
    item->setData(Qt::UserRole, filePath);
    listWidget->addItem(item);
}

void QuickLaunch::onItemDoubleClicked(QListWidgetItem *item)
{
    QString targetPath = item->data(Qt::UserRole).toString();
    if (!targetPath.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(targetPath));
}

// ================= 右键菜单与删除 =================
void QuickLaunch::showContextMenu(const QPoint &pos)
{
    QListWidget *listWidget = qobject_cast<QListWidget*>(sender());
    if (!listWidget) return;

    QListWidgetItem *item = listWidget->itemAt(pos);
    if (item) {
        QMenu menu(this);
        QAction *delAction = menu.addAction("删除该快捷方式");
        connect(delAction, &QAction::triggered, this, &QuickLaunch::deleteSelectedItem);
        menu.exec(listWidget->mapToGlobal(pos));
    }
}

void QuickLaunch::deleteSelectedItem()
{
    QListWidget *currentList = qobject_cast<QListWidget*>(tabWidget->currentWidget());
    if (currentList) {
        QListWidgetItem *item = currentList->currentItem();
        if (item) {
            delete item;
            saveCurrentTabPaths(); // 删除后立即保存状态
        }
    }
}

// ================= 拖放文件逻辑 =================
void QuickLaunch::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void QuickLaunch::dropEvent(QDropEvent *event)
{
    int currentIndex = tabWidget->currentIndex();
    QListWidget *currentList = qobject_cast<QListWidget*>(tabWidget->widget(currentIndex));
    if (!currentList) return;

    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString filePath = url.toLocalFile();
            if (!filePath.isEmpty()) {
                // 检查是否已经存在避免重复添加
                bool exists = false;
                for(int i = 0; i < currentList->count(); ++i) {
                    if (currentList->item(i)->data(Qt::UserRole).toString() == filePath) {
                        exists = true; break;
                    }
                }
                if (!exists) {
                    addAppToList(currentList, filePath);
                }
            }
        }
        saveCurrentTabPaths(); // 拖拽完成后统一保存当前页状态
        event->acceptProposedAction();
    }
}

// ================= 鼠标拖动与贴边隐藏逻辑 =================
void QuickLaunch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void QuickLaunch::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - dragPosition);
        event->accept();
    }
}

void QuickLaunch::mouseReleaseEvent(QMouseEvent *event)
{
    // 获取当前窗口中心点所在的屏幕（完美兼容多显示器）
    QScreen *screen = QGuiApplication::screenAt(geometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();

    QRect screenRect = screen->availableGeometry();
    QRect winRect = frameGeometry();

    // 如果松开鼠标时，窗口顶部距离该屏幕的顶部小于等于 5 像素
    if (winRect.top() <= screenRect.top() + 5) {
        isDockedTop = true;
        // 自动向上滑动隐藏，只留下 2 个像素作为鼠标感应区
        QRect hiddenRect = winRect;
        hiddenRect.moveTop(screenRect.top() - winRect.height() + 2);
        animateWindowGeometry(hiddenRect);
    } else {
        // 如果拖离了顶部，解除贴边状态
        isDockedTop = false;
    }

    QWidget::mouseReleaseEvent(event);
}

// 鼠标进入感应区：向下滑出
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void QuickLaunch::enterEvent(QEnterEvent *event)
#else
void QuickLaunch::enterEvent(QEvent *event)
#endif
{
    if (isDockedTop) {
        QScreen *screen = QGuiApplication::screenAt(geometry().center());
        if (!screen) screen = QGuiApplication::primaryScreen();

        QRect rect = geometry();
        // 移动到屏幕最顶端显示
        rect.moveTop(screen->availableGeometry().top());
        animateWindowGeometry(rect);
    }
    QWidget::enterEvent(event);
}

// 鼠标离开窗口区域：向上隐藏
void QuickLaunch::leaveEvent(QEvent *event)
{
    if (isDockedTop) {
        QScreen *screen = QGuiApplication::screenAt(geometry().center());
        if (!screen) screen = QGuiApplication::primaryScreen();

        QRect rect = geometry();
        // 缩回屏幕外，只留 2 个像素
        rect.moveTop(screen->availableGeometry().top() - rect.height() + 2);
        animateWindowGeometry(rect);
    }
    QWidget::leaveEvent(event);
}

// 丝滑移动的动画封装
void QuickLaunch::animateWindowGeometry(const QRect &endValue)
{
    QPropertyAnimation *anim = new QPropertyAnimation(this, "geometry");
    anim->setDuration(200); // 200毫秒的滑动时间，干脆利落
    anim->setEasingCurve(QEasingCurve::OutCubic); // 缓动曲线，视觉更平滑
    anim->setStartValue(geometry());
    anim->setEndValue(endValue);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ================= 关于对话框 =================
void QuickLaunch::showAboutDialog()
{
    // 使用 HTML 标签进行排版，打造专业外观
    QString aboutText = R"(
        <h2 style='color: #2c3e50;'>快速启动工具栏 (QuickLaunch)</h2>
        <p><b>当前版本:</b> v1.0.0 (64-bit)</p>
        <p><b>作者:</b> James Meng / BoilingIoT Studio</p>
        <hr>
        <p>这是一款轻量级、无广告、纯净的桌面效率提升工具。旨在帮助用户分类整理常用软件与文档，告别凌乱的电脑桌面。</p>
        <p><b>核心特性：</b></p>
        <ul>
            <li>✨ <b>极简设计：</b> 无边框沉浸式体验</li>
            <li>🖱️ <b>智能交互：</b> 拖拽添加、自动排序对齐</li>
            <li>🚀 <b>无感运行：</b> 顶部贴边隐藏，随叫随到</li>
        </ul>
        <hr>
        <p style='color: #7f8c8d; font-size: 12px;'>
        Copyright &copy; 2024. All rights reserved.<br>
        Powered by <a href='https://www.qt.io/'>Qt Framework</a> (C++).
        </p>
    )";

    // 调用 Qt 内置的关于对话框
    QMessageBox::about(this, "关于 快速启动工具栏", aboutText);
}