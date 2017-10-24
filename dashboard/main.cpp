#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>

#include "AppContext.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    if (QCoreApplication::arguments().size() != 2) {
        qFatal("Serial port name required (like 'ttyUSB0')");
        return -1;
    }

    qmlRegisterType<AppContext>("app", 1, 0, "AppContext");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/RpmDial.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
