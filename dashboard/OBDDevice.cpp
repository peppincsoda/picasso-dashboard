#include "OBDDevice.h"

#include <QtSerialPort/QSerialPort>
#include <QVariant>
#include <QTimer>

#include <cassert>
#include <queue>

namespace obdlib {

    class OBDDeviceImpl : public QObject
    {
        Q_OBJECT
    public:
        explicit OBDDeviceImpl(OBDDevice* interface);
        ~OBDDeviceImpl();

        bool open(const QString& name);
        void close();

        bool queryValue(OBDDevice::PID pid);

        void setLogOutput(void (*write_fn)(const char*));

    Q_SIGNALS:
        void queueOnOpenSignal(bool ok);
        void queueOnQueryValueSignal(bool ok, int pid, const QVariant& value);

    private Q_SLOTS:
        void onReadyRead();
        void onTimeout();

    private:
        enum class OBDBytes : char
        {
            ShowCurrentMode = 0x01,
            ReplyCurrentMode = 0x41,
        };

        struct Command
        {
            QString cmd_str_;
            bool (OBDDeviceImpl::*line_cb_)(const char *);
            void (OBDDeviceImpl::*ready_cb_)();
            void (OBDDeviceImpl::*error_cb_)();
        };

        void enqueueCommand(Command&& cmd);
        Q_INVOKABLE void processCmdQueue();

        void processResponse(const QByteArray& buffer);
        bool onATZLine(const char* line);
        void onATZReady();
        bool onOKLine(const char* line);
        void onATALReady();
        void onATSP0Ready();
        void openFailed();
        bool onQueryLine(const char* line);
        void onQueryReady();
        void queryFailed();

        OBDDevice* interface_;

        QSerialPort sp_;

        QByteArray read_buffer_;

        std::queue<Command> cmd_queue_;

        //! Last query PID
        int current_pid_;

        //! Timeout timer for the active command
        QTimer* timeout_timer_;

        void (*log_write_fn_)(const char*);
    };




    OBDDevice::OBDDevice(QObject* parent)
        : QObject(parent)
        , pimpl_(std::make_unique<OBDDeviceImpl>(this))
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

    void OBDDevice::setLogOutput(void (*write_fn)(const char *))
    {
        pimpl_->setLogOutput(write_fn);
    }

    OBDDeviceImpl::OBDDeviceImpl(OBDDevice* interface)
        : QObject()
        , interface_(interface)
        , sp_()
        , read_buffer_()
        , cmd_queue_()
        , current_pid_(OBDDevice::PID_Invalid)
        , timeout_timer_(nullptr)
        , log_write_fn_(nullptr)
    {
        connect(&sp_, SIGNAL(readyRead()),
                this, SLOT(onReadyRead()));

        timeout_timer_ = new QTimer(this);
        timeout_timer_->setInterval(10000);
        timeout_timer_->setSingleShot(true);
        connect(timeout_timer_, SIGNAL(timeout()),
                this, SLOT(onTimeout()));

        // All signals emitted by the device are deferred
        connect(this, SIGNAL(queueOnOpenSignal(bool)),
                interface_, SIGNAL(onOpen(bool)), Qt::QueuedConnection);
        connect(this, SIGNAL(queueOnQueryValueSignal(bool,int,QVariant)),
                interface_, SIGNAL(onQueryValue(bool,int,QVariant)), Qt::QueuedConnection);
    }

    OBDDeviceImpl::~OBDDeviceImpl()
    {

    }

    bool OBDDeviceImpl::open(const QString& name)
    {
        if (!cmd_queue_.empty())
            return false;
        if (sp_.isOpen())
            return false;

        sp_.setPortName(name);

        bool r = true;
        // Hardcoded parameters for ELM327 v1.5 (Chinese clone version)
        r = r && sp_.setBaudRate(QSerialPort::Baud38400);
        r = r && sp_.setDataBits(QSerialPort::Data8);
        r = r && sp_.setParity(QSerialPort::NoParity);
        r = r && sp_.setStopBits(QSerialPort::OneStop);
        r = r && sp_.setFlowControl(QSerialPort::NoFlowControl);

        r = r && sp_.open(QSerialPort::ReadWrite);
        enqueueCommand(Command {
                           "ATZ",
                            &OBDDeviceImpl::onATZLine,
                            &OBDDeviceImpl::onATZReady,
                            &OBDDeviceImpl::openFailed
                       });
        if (!r)
            sp_.close();
        return r;
    }

    void OBDDeviceImpl::enqueueCommand(Command &&cmd)
    {
        cmd_queue_.push(std::move(cmd));
        // Defer processing the queue to the next main loop iteration
        QMetaObject::invokeMethod(this, "processCmdQueue", Qt::QueuedConnection);
    }

    void OBDDeviceImpl::processCmdQueue()
    {
        assert(!cmd_queue_.empty());
        const auto& active_cmd = cmd_queue_.front();

        // Send command
        QByteArray buffer;
        buffer.append(active_cmd.cmd_str_);
        buffer.append('\r');
        if (sp_.write(buffer) != buffer.size()) {
            (this->*(active_cmd.error_cb_))();
            cmd_queue_.pop();
            return;
        }

        timeout_timer_->start();
    }

    void OBDDeviceImpl::onReadyRead()
    {
        auto new_buffer = sp_.readAll();

        // Print response to log
        if (log_write_fn_ != nullptr) {
            log_write_fn_(QString::fromLatin1(new_buffer.constData())
                          .replace("\r", "\r\n")
                          .toLatin1()
                          .constData());
        }

        read_buffer_.append(std::move(new_buffer));

        for (;;) {
            // Look for the prompt sign
            const auto len = read_buffer_.indexOf("\r>");
            if (len == -1)
                break;

            if (len > 0) {
                processResponse(read_buffer_.left(len));
            }
            read_buffer_.remove(0, len + 2);
        }
    }

    void OBDDeviceImpl::processResponse(const QByteArray& buffer)
    {
        assert(!cmd_queue_.empty());
        const auto& active_cmd = cmd_queue_.front();

        auto error = false;
        timeout_timer_->stop();

        // Process all non-empty lines
        const auto& lines = buffer.split('\r');
        for (const auto& line : lines) {
            if (!line.isEmpty()) {
                // Skip echo from the device
                if (line == active_cmd.cmd_str_)
                    continue;

                if (!(this->*(active_cmd.line_cb_))(line)) {
                    error = true;
                    break;
                }
            }
        }

        if (error) {
            (this->*(active_cmd.error_cb_))();
        } else {
            (this->*(active_cmd.ready_cb_))();
        }

        cmd_queue_.pop();
    }

    void OBDDeviceImpl::onTimeout()
    {
        assert(!cmd_queue_.empty());
        const auto& active_cmd = cmd_queue_.front();

        (this->*(active_cmd.error_cb_))();
        cmd_queue_.pop();
    }

    bool OBDDeviceImpl::onATZLine(const char* line)
    {
        static const char id_str[] = "ELM327";

        return strncmp(line, id_str, sizeof(id_str) - 1) == 0;
    }

    void OBDDeviceImpl::onATZReady()
    {
        enqueueCommand(Command {
                           "ATAL",
                            &OBDDeviceImpl::onOKLine,
                            &OBDDeviceImpl::onATALReady,
                            &OBDDeviceImpl::openFailed
                       });
    }

    bool OBDDeviceImpl::onOKLine(const char* line)
    {
        return strcmp(line, "OK") == 0;
    }

    void OBDDeviceImpl::onATALReady()
    {
        enqueueCommand(Command {
                           "ATSP0",
                            &OBDDeviceImpl::onOKLine,
                            &OBDDeviceImpl::onATSP0Ready,
                            &OBDDeviceImpl::openFailed
                       });
    }

    void OBDDeviceImpl::onATSP0Ready()
    {
        emit queueOnOpenSignal(true);
    }

    void OBDDeviceImpl::openFailed()
    {
        sp_.close();
        emit queueOnOpenSignal(false);
    }

    void OBDDeviceImpl::close()
    {
        sp_.close();
        read_buffer_.clear();
        while (!cmd_queue_.empty())
            cmd_queue_.pop();
        timeout_timer_->stop();
    }

    bool OBDDeviceImpl::queryValue(OBDDevice::PID pid)
    {
        if (!cmd_queue_.empty())
            return false;
        if (!sp_.isOpen())
            return false;
        if (sp_.error() != QSerialPort::NoError)
            return false;

        current_pid_ = pid;

        auto cmd = QString("%1%2")
                .arg(static_cast<char>(OBDBytes::ShowCurrentMode), 2, 16, QChar('0'))
                .arg(static_cast<char>(pid), 2, 16, QChar('0'))
                .toLatin1();

        enqueueCommand(Command {
                           std::move(cmd),
                            &OBDDeviceImpl::onQueryLine,
                            &OBDDeviceImpl::onQueryReady,
                            &OBDDeviceImpl::queryFailed
                       });
        return true;
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

    bool OBDDeviceImpl::onQueryLine(const char* line)
    {
        if (strcmp(line, "SEARCHING...") == 0)
            return true;
        if (strcmp(line, "UNABLE TO CONNECT") == 0)
            return false;
        if (strcmp(line, "BUS ERROR") == 0)
            return false;
        if (strcmp(line, "NO DATA") == 0) {
            emit queueOnQueryValueSignal(true, current_pid_, QVariant());
            return true;
        }

        const auto buffer = readHexBytes(line);
        const auto len = buffer.size();

        if (len >= 2) {
            const auto mode = static_cast<OBDBytes>(buffer[0]);
            const auto pid = static_cast<OBDDevice::PID>(buffer[1]);
            if (mode == OBDBytes::ReplyCurrentMode) {
                QVariant value;

                switch (pid) {
                case OBDDevice::PID_EngineRPM:
                    if (len >= 4)
                        value = (static_cast<std::uint8_t>(buffer[2]) * 256 +
                                static_cast<std::uint8_t>(buffer[3])) / 4;
                    break;
                case OBDDevice::PID_VehicleSpeed:
                    if (len >= 3)
                        value = static_cast<std::uint8_t>(buffer[2]);
                    break;
                default:
                    assert(0);
                    break;
                }

                if (!value.isNull()) {
                    emit queueOnQueryValueSignal(true, pid, value);
                    return true;
                }
            }
        }

        return false;
    }

    void OBDDeviceImpl::onQueryReady()
    {

    }

    void OBDDeviceImpl::queryFailed()
    {
        sp_.close();
        emit queueOnQueryValueSignal(false, current_pid_, QVariant());
    }

    void OBDDeviceImpl::setLogOutput(void (*write_fn)(const char*))
    {
        log_write_fn_ = write_fn;
    }
}

#include "OBDDevice.moc"
