#include "QuickLaunch.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // ================== 新增：高 DPI 多显示器缩放支持 ==================
    // 注意：这些属性必须在创建 QApplication 对象之前设置！

    // 兼容 Qt5 的写法（Qt6 默认已开启这些功能）
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 允许根据显示器的缩放比例自动缩放 UI 元素
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // 强制使用高分辨率的图标，防止拖到高分屏上图标变模糊
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    // =================================================================
    QApplication a(argc, argv);
    QuickLaunch w;
    w.show();
    return a.exec();
}