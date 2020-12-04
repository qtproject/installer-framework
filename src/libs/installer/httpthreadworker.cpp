#include "httpthreadworker.h"
#include <QTimer>
#include <QThread>
#include <QObject>

HttpThreadWorker::HttpThreadWorker()
{
    m_started = 0;
    m_finished = 0;
}

void HttpThreadWorker::process(const QByteArray& data, const QString& url, const QString& contentType)
{
    qDebug() << "framework | HttpThreadWorker::process |" << data;
    m_started++;

    QNetworkRequest* request = new QNetworkRequest();

    if(!m_network)
    {
        m_network.reset(new QNetworkAccessManager());
    }

    request->setUrl(url);
    request->setHeader(QNetworkRequest::ContentTypeHeader, contentType);

    QByteArray empty;

    QNetworkReply* reply = m_network->post(*request, data);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        QString status = status_code.toString();
        qDebug() << "framework | HttpThreadWorker::process | status =" << status;

        m_finished++; reply->deleteLater();
    } );
    void (QNetworkReply::*errorSignal)(QNetworkReply::NetworkError) = &QNetworkReply::error;
    connect(reply, errorSignal, [this, reply]() {
        qWarning() << "framework | HttpThreadWorker::process | Failed to send POST";
    } );

    qDebug() << "framework | HttpThreadWorker::process | POST from worker thread " << thread();
}

bool HttpThreadWorker::isRunning()
{
    return m_started > m_finished;
}
