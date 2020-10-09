#ifndef HTTPTHREADCONTROLLER_H
#define HTTPTHREADCONTROLLER_H

#include <QObject>
#include "httpthreadworker.h"

class HttpThreadController : public QObject
{
    Q_OBJECT
private:
    QThread m_workerThread;
    HttpThreadWorker* m_worker;

public:
    explicit HttpThreadController(QObject *parent = nullptr);
    void lastChanceToFinish(uint maxWaitMilliseconds = 1000);

    ~HttpThreadController();

signals:
    void postTelemetry(const QByteArray&, const QString&);
public slots:
};

#endif // HTTPTHREADCONTROLLER_H
