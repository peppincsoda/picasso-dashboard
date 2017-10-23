#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "AppContext.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<AppContext>("app", 1, 0, "AppContext");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/RpmDial.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
