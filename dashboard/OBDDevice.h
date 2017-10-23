#ifndef OBDDEVICE_H
#define OBDDEVICE_H

#include <QObject>

#include <memory>

namespace obdlib {

    class OBDDevice : public QObject
    {
        Q_OBJECT
    public:
        enum PID
        {
            PID_Invalid = -1,

            PID_EngineRPM = 12,
            PID_VehicleSpeed = 13,
        };

        explicit OBDDevice(QObject* parent = nullptr);
        ~OBDDevice();

        bool open(const QString& portName);
        void close();

        bool queryValue(PID pid);

    Q_SIGNALS:
        void onOpen(bool ok);
        void onQueryValue(bool ok, int pid, const QVariant& value);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}


#endif // OBDDEVICE_H
