#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <QDebug>

#include "AppContext.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("dashboard");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main",
                                                                 "Shows an RPM Gauge using the OBDII interface."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("serial-port", QCoreApplication::translate("main", "Serial port name (like COM1 or ttyUSB0)."));

    QCommandLineOption quietOption("q", QCoreApplication::translate("main", "Do not dump serial data to the console"));
    parser.addOption(quietOption);

    parser.process(app);

    const auto& args = parser.positionalArguments();
    if (args.size() != 1) {
        qFatal(QCoreApplication::translate("main", "Serial port name required (like COM1 or ttyUSB0)").toUtf8());
        return -1;
    }

    AppContext::setQuietOption(parser.isSet(quietOption));

    qmlRegisterType<AppContext>("app", 1, 0, "AppContext");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/RpmDial.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
