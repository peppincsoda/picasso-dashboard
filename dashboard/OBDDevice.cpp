#include "OBDDevice.h"

#include <QtSerialPort/QSerialPort>
#include <QVariant>

#include <cassert>

namespace obdlib {

    class OBDDevice::Impl : public QObject
    {
        Q_OBJECT
    public:
        explicit Impl(OBDDevice* interface);
        ~Impl();

        bool open(const QString& name);
        void close();

        bool queryValue(PID pid);

    private Q_SLOTS:
        void onReadyRead();

    private:
        bool send(const char* str, bool (Impl::*cb)(const char*));

        void processLine(const char* str);
        bool onATZ(const char* resp);
        bool onATAL(const char* resp);
        bool onATSP0(const char* resp);
        void openFailed();
        bool onQuery(const char* resp);
        void queryFailed();

        OBDDevice* interface_;

        QSerialPort sp_;

        QByteArray read_buffer_;

        // Callback's return value indicates whether
        // the current callback should be cleared
        bool (OBDDevice::Impl::*current_cb_)(const char*);
    };


    enum class OBDBytes : char
    {
        ShowCurrentMode = 0x01,
        ReplyCurrentMode = 0x41,
    };


    OBDDevice::OBDDevice(QObject* parent)
        : QObject(parent)
        , pimpl_(std::make_unique<Impl>(this))
    {

    }

    OBDDevice::~OBDDevice()
    {

    }

    bool OBDDevice::open(const QString& portName)
    {
        return pimpl_->open(portName);
    }

    void OBDDevice::close()
    {
        pimpl_->close();
    }

    bool OBDDevice::queryValue(PID pid)
    {
        return pimpl_->queryValue(pid);
    }

    OBDDevice::Impl::Impl(OBDDevice* interface)
        : QObject()
        , interface_(interface)
        , sp_()
        , read_buffer_()
        , current_cb_(nullptr)
    {
        connect(&sp_, SIGNAL(readyRead()),
                this, SLOT(onReadyRead()));
    }

    OBDDevice::Impl::~Impl()
    {

    }

    bool OBDDevice::Impl::open(const QString& name)
    {
        if (current_cb_ != nullptr)
            return false;
        if (sp_.isOpen())
            return false;

        sp_.setPortName(name);

        bool r = true;
        r = r && sp_.setBaudRate(QSerialPort::Baud38400);
        r = r && sp_.setDataBits(QSerialPort::Data8);
        r = r && sp_.setParity(QSerialPort::EvenParity);
        r = r && sp_.setStopBits(QSerialPort::OneStop);
        r = r && sp_.setFlowControl(QSerialPort::NoFlowControl);

        r = r && sp_.open(QSerialPort::ReadWrite);
        r = r && send("ATZ", &Impl::onATZ);
        if (!r)
            sp_.close();
        return r;
    }

    bool OBDDevice::Impl::send(const char* str, bool (Impl::*cb)(const char*))
    {
        assert(current_cb_ == nullptr);

        QByteArray buffer;
        buffer.append(str); // TODO: TEST
        buffer.append('\r');

        if (sp_.write(buffer) != buffer.size())
            return false;

        current_cb_ = cb;
        return true;
    }

    void OBDDevice::Impl::onReadyRead()
    {
        const auto buffer_size = read_buffer_.size();
        read_buffer_.append(sp_.readAll());
        const auto len = read_buffer_.indexOf('\r', buffer_size);
        if (len != -1) {
            read_buffer_[len] = 0;
            processLine(read_buffer_.constData());
            read_buffer_.remove(0, len + 1);
        }
    }

    void OBDDevice::Impl::processLine(const char* str)
    {
        assert(current_cb_ != nullptr);
        const auto current_cb = current_cb_;
        current_cb_ = nullptr;
        if (!(this->*current_cb)(str))
            current_cb_ = current_cb; // Keep the current operation
    }

    bool OBDDevice::Impl::onATZ(const char* resp)
    {
        if (strcmp(resp, "OK") != 0 ||
            !send("ATAL", &Impl::onATAL))
                openFailed();
        return true;
    }

    bool OBDDevice::Impl::onATAL(const char* resp)
    {
        if (strcmp(resp, "OK") != 0 ||
            !send("ATSP0", &Impl::onATSP0))
                openFailed();
        return true;
    }

    bool OBDDevice::Impl::onATSP0(const char* resp)
    {
        if (strcmp(resp, "OK") != 0)
            openFailed();

        emit interface_->onOpen(true);
        return true;
    }

    void OBDDevice::Impl::openFailed()
    {
        sp_.close();
        emit interface_->onOpen(false);
    }

    void OBDDevice::Impl::close()
    {
        sp_.close();
        current_cb_ = nullptr;
    }

    bool OBDDevice::Impl::queryValue(PID pid)
    {
        if (current_cb_ != nullptr)
            return false;
        if (sp_.isOpen())
            return false;
        if (sp_.error() != QSerialPort::NoError)
            return false;

        const auto cmd = QString("%1%2")
                .arg(static_cast<char>(OBDBytes::ShowCurrentMode), 2, 16, QChar('0'))
                .arg(static_cast<char>(pid), 2, 16, QChar('0'))
                .toLatin1();

        return send(cmd.data(), &Impl::onQuery);
    }

    static inline int getHexDigitValue(char c)
    {
        if ('0' <= c && c <= '9')
            return c - '0';
        else if ('A' <= c && c <= 'F')
            return c - 'A' + 10;
        else if ('a' <= c && c <= 'f')
            return c - 'a' + 10;
        return -1;
    }

    static QByteArray readHexBytes(const char* str)
    {
        QByteArray r;
        while (*str != 0) {
            while (*str == ' ')
                str++;

            const auto d1 = getHexDigitValue(*str++);
            if (d1 == -1)
                return r;
            const auto d2 = getHexDigitValue(*str++);
            if (d2 == -1)
                return r;

            r.append((d1 << 4) | d2);
        }
        return r;
    }

    bool OBDDevice::Impl::onQuery(const char* resp)
    {
        if (strcmp(resp, "SEARCHING...") == 0)
            return false;

        const auto buffer = readHexBytes(resp);
        const auto len = buffer.size();

        if (len >= 2) {
            const auto mode = static_cast<OBDBytes>(buffer[0]);
            const auto pid = static_cast<PID>(buffer[1]);
            if (mode == OBDBytes::ReplyCurrentMode) {
                QVariant value;

                switch (pid) {
                case PID_EngineRPM:
                    if (len >= 4)
                        value = (buffer[2] * 256 + buffer[3]) / 4;
                    break;
                case PID_VehicleSpeed:
                    if (len >= 3)
                        value = buffer[2];
                    break;
                default:
                    assert(0);
                    break;
                }

                if (!value.isNull()) {
                    emit interface_->onQueryValue(true, pid, value);
                } else {
                    queryFailed();
                }
            }
        } else {
            queryFailed();
        }
        return true;
    }

    void OBDDevice::Impl::queryFailed()
    {
        sp_.close();
        emit interface_->onQueryValue(false, PID_Invalid, QVariant());
    }
}

#include "OBDDevice.moc"
