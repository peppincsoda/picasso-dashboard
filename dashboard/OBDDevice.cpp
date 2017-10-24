#include "OBDDevice.h"

#include <QtSerialPort/QSerialPort>
#include <QVariant>
#include <QTimer>

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

    Q_SIGNALS:
        void internalSignalOnOpen(bool ok);
        void internalSignalOnQueryValue(bool ok, int pid, const QVariant& value);

    private Q_SLOTS:
        void onReadyRead();
        void onTimeout();

    private:
        bool send(const char* cmd,
                  bool (Impl::*line_cb)(const char *),
                  bool (Impl::*ready_cb)(),
                  void (Impl::*error_cb)());

        void processLine(const char* line);
        void processPrompt();
        bool onATZLine(const char* line);
        bool onATZReady();
        bool onOKLine(const char* line);
        bool onATALReady();
        bool onATSP0Ready();
        void openFailed();
        bool onQueryLine(const char* line);
        bool onQueryReady();
        void queryFailed();

        OBDDevice* interface_;

        QSerialPort sp_;

        QByteArray read_buffer_;

        //! Last command to recognize echo
        QByteArray current_cmd_;
        //! Last query PID
        int current_pid_;

        bool (Impl::*line_cb_)(const char *);
        bool (Impl::*ready_cb_)();
        void (Impl::*error_cb_)();
        bool error_on_read_;

        //! Timeout timer for the active command
        QTimer* timeout_timer_;
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
        , current_cmd_()
        , current_pid_(PID_Invalid)
        , line_cb_(nullptr)
        , ready_cb_(nullptr)
        , error_cb_(nullptr)
        , error_on_read_(false)
        , timeout_timer_(nullptr)
    {
        connect(&sp_, SIGNAL(readyRead()),
                this, SLOT(onReadyRead()));

        timeout_timer_ = new QTimer(this);
        timeout_timer_->setInterval(3000);
        timeout_timer_->setSingleShot(true);
        connect(timeout_timer_, SIGNAL(timeout()),
                this, SLOT(onTimeout()));

        // All signals emitted by the device are deferred
        connect(this, SIGNAL(internalSignalOnOpen(bool)),
                interface_, SIGNAL(onOpen(bool)), Qt::QueuedConnection);
        connect(this, SIGNAL(internalSignalOnQueryValue(bool,int,QVariant)),
                interface_, SIGNAL(onQueryValue(bool,int,QVariant)), Qt::QueuedConnection);
    }

    OBDDevice::Impl::~Impl()
    {

    }

    bool OBDDevice::Impl::open(const QString& name)
    {
        if (line_cb_ != nullptr)
            return false;
        if (sp_.isOpen())
            return false;

        sp_.setPortName(name);

        bool r = true;
        r = r && sp_.setBaudRate(QSerialPort::Baud38400);
        r = r && sp_.setDataBits(QSerialPort::Data8);
        r = r && sp_.setParity(QSerialPort::NoParity);
        r = r && sp_.setStopBits(QSerialPort::OneStop);
        r = r && sp_.setFlowControl(QSerialPort::NoFlowControl);

        r = r && sp_.open(QSerialPort::ReadWrite);
        r = r && send("ATZ",
                      &Impl::onATZLine,
                      &Impl::onATZReady,
                      &Impl::openFailed);
        if (!r)
            sp_.close();
        return r;
    }

    bool OBDDevice::Impl::send(const char* cmd,
                               bool (Impl::*line_cb)(const char *),
                               bool (Impl::*ready_cb)(),
                               void (Impl::*error_cb)())
    {
        assert(line_cb_ == nullptr);
        assert(ready_cb_ == nullptr);
        assert(error_cb_ == nullptr);

        QByteArray buffer;
        buffer.append(cmd);
        buffer.append('\r');
        if (sp_.write(buffer) != buffer.size())
            return false;

        current_cmd_ = cmd;
        line_cb_ = line_cb;
        ready_cb_ = ready_cb;
        error_cb_ = error_cb;
        timeout_timer_->start();
        return true;
    }

    void OBDDevice::Impl::onReadyRead()
    {
        // Process all non-empty response lines one-by-one
        int from = read_buffer_.size();
        read_buffer_.append(sp_.readAll());
        for (;;) {
            const auto len = read_buffer_.indexOf('\r', from);
            if (len == -1)
                break;

            if (len > 0) {
                read_buffer_[len] = 0;
                processLine(read_buffer_.constData());
            }
            read_buffer_.remove(0, len + 1);
            from = 0;
        }

        // Process prompt sign
        if (read_buffer_.size() == 1 &&
            read_buffer_[0] == '>') {
            read_buffer_.clear();
            processPrompt();
        }
    }

    void OBDDevice::Impl::processLine(const char* line)
    {
        assert(line_cb_ != nullptr);
        assert(error_cb_ != nullptr);

        // Skip echo from the device
        if (strcmp(line, current_cmd_.constData()) == 0)
            return;

        if (!error_on_read_) {
            if (!(this->*line_cb_)(line)) {
                error_on_read_ = true;
                (this->*error_cb_)();
            }
        }
    }

    void OBDDevice::Impl::processPrompt()
    {
        assert(ready_cb_ != nullptr);
        assert(error_cb_ != nullptr);

        timeout_timer_->stop();

        line_cb_ = nullptr;
        const auto ready_cb = ready_cb_;
        ready_cb_ = nullptr;
        const auto error_cb = error_cb_;
        error_cb_ = nullptr;

        if (!error_on_read_) {
            if (!(this->*ready_cb)()) {
                error_on_read_ = true;
                (this->*error_cb)();
            }
        }

        // Command processing is finished here, reset the error flag
        error_on_read_ = false;
    }

    void OBDDevice::Impl::onTimeout()
    {
        error_on_read_ = true;
        (this->*error_cb_)();
    }

    bool OBDDevice::Impl::onATZLine(const char* line)
    {
        static const char id_str[] = "ELM327";

        return strncmp(line, id_str, sizeof(id_str) - 1) == 0;
    }

    bool OBDDevice::Impl::onATZReady()
    {
        return send("ATAL",
                    &Impl::onOKLine,
                    &Impl::onATALReady,
                    &Impl::openFailed);
    }

    bool OBDDevice::Impl::onOKLine(const char* line)
    {
        return strcmp(line, "OK") == 0;
    }

    bool OBDDevice::Impl::onATALReady()
    {
        return send("ATSP0",
                    &Impl::onOKLine,
                    &Impl::onATSP0Ready,
                    &Impl::openFailed);
    }

    bool OBDDevice::Impl::onATSP0Ready()
    {
        emit internalSignalOnOpen(true);
        return true;
    }

    void OBDDevice::Impl::openFailed()
    {
        sp_.close();
        emit internalSignalOnOpen(false);
    }

    void OBDDevice::Impl::close()
    {
        sp_.close();
        read_buffer_.clear();
        current_cmd_.clear();
        line_cb_ = nullptr;
        ready_cb_ = nullptr;
        error_cb_ = nullptr;
        timeout_timer_->stop();
    }

    bool OBDDevice::Impl::queryValue(PID pid)
    {
        if (line_cb_ != nullptr)
            return false;
        if (!sp_.isOpen())
            return false;
        if (sp_.error() != QSerialPort::NoError)
            return false;

        current_pid_ = pid;

        const auto cmd = QString("%1%2")
                .arg(static_cast<char>(OBDBytes::ShowCurrentMode), 2, 16, QChar('0'))
                .arg(static_cast<char>(pid), 2, 16, QChar('0'))
                .toLatin1();

        return send(cmd.data(),
                    &Impl::onQueryLine,
                    &Impl::onQueryReady,
                    &Impl::queryFailed);
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

    bool OBDDevice::Impl::onQueryLine(const char* line)
    {
        if (strcmp(line, "SEARCHING...") == 0)
            return true;
        if (strcmp(line, "UNABLE TO CONNECT") == 0)
            return false;

        const auto buffer = readHexBytes(line);
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
                    emit internalSignalOnQueryValue(true, pid, value);
                    return true;
                }
            }
        }

        return false;
    }

    bool OBDDevice::Impl::onQueryReady()
    {
        return true;
    }

    void OBDDevice::Impl::queryFailed()
    {
        sp_.close();
        emit internalSignalOnQueryValue(false, current_pid_, QVariant());
    }
}

#include "OBDDevice.moc"
