#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <QObject>

#include <memory>

class QTimer;

namespace obdlib {
    class OBDDevice;
}

class AppContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rpmValue READ rpmValue NOTIFY rpmValueChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    AppContext(QObject* parent = nullptr);
    ~AppContext();

    int rpmValue() const;
    QString message() const;

    Q_INVOKABLE void start();

Q_SIGNALS:
    void rpmValueChanged(int rpmValue);
    void messageChanged(QString message);

private Q_SLOTS:
    void onDeviceOpen(bool ok);
    void onDeviceQuery(bool ok, int pid, const QVariant& value);
    void onErrorTimer();
    void onQueryTimer();

private:
    void setRpmValue(int rpmValue);
    void setMessage(QString message);

    void tryConnect();
    void startErrorTimeout(const QString& error_message);
    void updateErrorMessage();
    void queryFailed();

    int rpmValue_;
    QString message_;

    std::unique_ptr<obdlib::OBDDevice> device_;

    QTimer* error_timer_;
    int remaining_seconds_;
    QString error_message_;

    bool querying_;
    QTimer* query_timer_;
};

#endif // APPCONTEXT_H
