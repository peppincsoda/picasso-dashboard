#include "AppContext.h"

#include <QTimer>
#include <QVariant>
#include <QCoreApplication>

#include "OBDDevice.h"

#include <iostream>

bool AppContext::quietOption_ = false;

void AppContext::setQuietOption(bool quietOption)
{
    quietOption_ = quietOption;
}

static void write_to_stdout(const char* str)
{
    std::cout << str;
}

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , rpmValue_(0)
    , message_()
    , fpsValue_(0)
    , device_(std::make_unique<obdlib::OBDDevice>())
    , error_timer_(nullptr)
    , remaining_seconds_(0)
    , error_message_()
    , fps_timer_(nullptr)
    , num_queries_(0)
{
    connect(device_.get(), SIGNAL(onOpen(bool)),
            this, SLOT(onDeviceOpen(bool)));
    connect(device_.get(), SIGNAL(onQueryValue(bool, int, const QVariant&)),
            this, SLOT(onDeviceQuery(bool, int, const QVariant&)));

    error_timer_ = new QTimer(this);
    error_timer_->setInterval(1000);
    connect(error_timer_, SIGNAL(timeout()),
            this, SLOT(onErrorTimer()));
    fps_timer_ = new QTimer(this);
    fps_timer_->setInterval(1000);
    connect(fps_timer_, SIGNAL(timeout()),
            this, SLOT(onFpsTimer()));
    fps_timer_->start();

    if (!quietOption_) {
        device_->setLogOutput(write_to_stdout);
    }
}

AppContext::~AppContext()
{

}

int AppContext::rpmValue() const
{
    return rpmValue_;
}

QString AppContext::message() const
{
    return message_;
}

int AppContext::fpsValue() const
{
    return fpsValue_;
}

void AppContext::setRpmValue(int rpmValue)
{
    if (rpmValue_ != rpmValue) {
        rpmValue_ = rpmValue;
        emit rpmValueChanged(rpmValue);
    }
}

void AppContext::setMessage(QString message)
{
    if (message_ != message) {
        message_ = message;
        emit messageChanged(message);
    }
}

void AppContext::setFpsValue(int fpsValue)
{
    if (fpsValue_ != fpsValue) {
        fpsValue_ = fpsValue;
        emit fpsValueChanged(fpsValue);
    }
}

void AppContext::tryConnect()
{
    const auto portName = QCoreApplication::arguments().at(1);
    if (!device_->open(portName)) {
        openFailed();
        return;
    }

    setMessage(tr("Opening port..."));
}

void AppContext::startErrorTimeout(const QString& error_message)
{
    error_message_ = error_message;
    error_timer_->start();
    remaining_seconds_ = 5;
    updateErrorMessage();
}

void AppContext::onErrorTimer()
{
    remaining_seconds_ -= 1;
    if (remaining_seconds_ > 0) {
        updateErrorMessage();
    } else {
        error_timer_->stop();
        tryConnect();
    }
}

void AppContext::updateErrorMessage()
{
    setMessage(tr("ERROR: %1,<br>reconnecting in %2 second(s)...")
               .arg(error_message_, QString::number(remaining_seconds_)));
}

void AppContext::onDeviceOpen(bool ok)
{
    if (!ok) {
        openFailed();
        return;
    }

    setMessage("");

    if (!device_->queryValue(obdlib::OBDDevice::PID_EngineRPM)) {
        queryFailed();
        return;
    }
}

void AppContext::onDeviceQuery(bool ok, int /* pid */, const QVariant& value)
{
    if (!ok) {
        queryFailed();
        return;
    }

    setRpmValue(value.toInt());
    num_queries_++;

    if (!device_->queryValue(obdlib::OBDDevice::PID_EngineRPM)) {
        queryFailed();
        return;
    }
}

void AppContext::openFailed()
{
    const auto portName = QCoreApplication::arguments().at(1);
    startErrorTimeout(tr("Cannot open port: %1").arg(portName));
}

void AppContext::queryFailed()
{
    device_->close();
    startErrorTimeout(tr("Parameter query failed"));
}

void AppContext::start()
{
    tryConnect();
}

void AppContext::onFpsTimer()
{
    setFpsValue(num_queries_);
    num_queries_ = 0;
}
