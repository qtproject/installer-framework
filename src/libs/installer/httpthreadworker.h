#ifndef HTTPTHREADWORKER_H
#define HTTPTHREADWORKER_H

#include <QObject>
#include <QThread>
#include <QQueue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
class HttpThreadWorker: public QThread
{
    Q_OBJECT
public:
    HttpThreadWorker();
    bool isRunning();

public slots:
    void process(const QByteArray& data, const QString& url, const QString& contentType);
private:
    QSharedPointer<QNetworkAccessManager> m_network;
    int m_started;
    int m_finished;
};

#endif // HTTPTHREADCONTEXT_H
