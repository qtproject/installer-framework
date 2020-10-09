#include "httpthreadcontroller.h"
#include <QThread>
#include <QEventLoop>


HttpThreadController::HttpThreadController(QObject *parent) : QObject(parent)
{
    m_worker =new HttpThreadWorker;
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &HttpThreadController::postTelemetry, m_worker, &HttpThreadWorker::process);
    m_workerThread.start();
}

void HttpThreadController::lastChanceToFinish(uint maxWaitMilliseconds)
{
    //gives worker chance to start http requests that were already queued
    QThread::msleep(10);
    if(m_worker->isRunning())
    {
        //gives worker chance to finish it's non finished http requests
        QThread::msleep(maxWaitMilliseconds);
    }
}

HttpThreadController::~HttpThreadController()
{
    m_workerThread.quit();
    m_workerThread.wait();
}
