#include "ui/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral(MP_APP_NAME));
    QApplication::setOrganizationName(QStringLiteral("mareg74"));
    QApplication::setOrganizationDomain(QStringLiteral("mareg74.com"));
    QApplication::setApplicationVersion(QStringLiteral(MP_VERSION_STRING));

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationDisplayName(QStringLiteral(MP_APP_NAME));

    MainWindow window;
    window.show();
    return app.exec();
}
