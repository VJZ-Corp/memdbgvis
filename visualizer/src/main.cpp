#include "debugvisualizer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    DebugVisualizer window;

    // adjust for screen size
    if (QGuiApplication::primaryScreen()->geometry().width() <= 1920)
        window.setWindowState(Qt::WindowMaximized);

    window.show();
    return QApplication::exec();
}