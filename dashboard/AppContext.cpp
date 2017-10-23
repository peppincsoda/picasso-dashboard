#include "AppContext.h"

#include <QSerialPortInfo>
#include <QTimer>
#include <QVariant>

#include "OBDDevice.h"

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , rpmValue_(0)
    , message_()
    , device_(std::make_unique<obdlib::OBDDevice>())
    , error_timer_(nullptr)
    , remaining_seconds_(0)
    , error_message_()
    , query_timer_(nullptr)
{
    connect(device_.get(), SIGNAL(onOpen(bool)),
            this, SLOT(onDeviceOpen(bool)));

    error_timer_ = new QTimer(this);
    connect(error_timer_, SIGNAL(timeout()),
            this, SLOT(onErrorTimer()));
    query_timer_ = new QTimer(this);
    connect(query_timer_, SIGNAL(timeout()),
            this, SLOT(onQueryTimer()));
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

void AppContext::tryConnect()
{
    const auto& ports = QSerialPortInfo::availablePorts();
    if (ports.size() != 1) {
        startErrorTimeout(tr("No serial ports found"));
        return;
    }

    const auto& port = ports.first();
    if (!device_->open(port.portName())) {
        startErrorTimeout(tr("Cannot open port: %1").arg(port.portName()));
        return;
    }

    setMessage(tr("Initializing..."));
}

void AppContext::startErrorTimeout(const QString& error_message)
{
    error_message_ = error_message;
    error_timer_->start(1000);
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
    setMessage(tr("ERROR: %1,<br>retrying in %2 second(s)...")
               .arg(error_message_, QString::number(remaining_seconds_)));
}

void AppContext::onDeviceOpen(bool ok)
{
    if (!ok) {
        tryConnect();
        return;
    }

    setMessage("");
    query_timer_->start(100);
}

void AppContext::onQueryTimer()
{
    if (!device_->queryValue(obdlib::OBDDevice::PID_EngineRPM)) {
        queryFailed();
        return;
    }
}

void AppContext::onDeviceQuery(bool ok, int pid, const QVariant& value)
{
    if (!ok) {
        queryFailed();
        return;
    }

    if (pid != obdlib::OBDDevice::PID_EngineRPM)
        return;

    setRpmValue(value.toInt());
}

void AppContext::queryFailed()
{
    query_timer_->stop();
    device_->close();
    startErrorTimeout(tr("Parameter query failed"));
}

void AppContext::start()
{
    tryConnect();
}
