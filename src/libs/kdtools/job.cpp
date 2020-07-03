/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "job.h"

#include <QDebug>
#include <QEventLoop>
#include <QTimer>

#include "globals.h"

// -- Job::Private

class Job::Private
{
    Job *const q;

public:
    explicit Private(Job *qq)
        : q(qq)
        , error(Job::NoError)
        , caps(Job::NoCapabilities)
        , autoDelete(true)
        , totalAmount(100)
        , processedAmount(0)
        , m_timeout(-1)
    {
        connect(&m_timer, &QTimer::timeout, q, &Job::cancel);
    }

    ~Private()
    {
        m_timer.stop();
    }

    void delayedStart()
    {
        q->doStart();
        emit q->started(q);
    }

    void waitForSignal(const char *sig)
    {
        QEventLoop loop;
        q->connect(q, sig, &loop, SLOT(quit()));

        if (m_timeout >= 0)
            m_timer.start(m_timeout);
        else
            m_timer.stop();

        loop.exec();
    }

    int error;
    QString errorString;
    Job::Capabilities caps;
    bool autoDelete;
    quint64 totalAmount;
    quint64 processedAmount;
    int m_timeout;
    QTimer m_timer;
};


// -- Job

Job::Job(QObject *parent)
    : QObject(parent),
      d(new Private(this))
{
    connect(this, &Job::finished, this, &Job::onFinished);
}

Job::~Job()
{
    delete d;
}

bool Job::autoDelete() const
{
    return d->autoDelete;
}

void Job::setAutoDelete(bool autoDelete)
{
    d->autoDelete = autoDelete;
}

int Job::error() const
{
    return d->error;
}

QString Job::errorString() const
{
    return d->errorString;
}

void Job::emitFinished()
{
    emit finished(this);
}

void Job::emitFinishedWithError(int error, const QString &errorString)
{
    d->error = error;
    d->errorString = errorString;
    emitFinished();
}

void Job::setError(int error)
{
    d->error = error;
}

void Job::setErrorString(const QString &errorString)
{
    d->errorString = errorString;
}

void Job::waitForStarted()
{
    d->waitForSignal(SIGNAL(started(Job*)));
}

void Job::waitForFinished()
{
    d->waitForSignal(SIGNAL(finished(Job*)));
}

Job::Capabilities Job::capabilities() const
{
    return d->caps;
}

bool Job::hasCapability(Capability c) const
{
    return d->caps.testFlag(c);
}

void Job::setCapabilities(Capabilities c)
{
    d->caps = c;
}

void Job::start()
{
    QMetaObject::invokeMethod(this, "delayedStart", Qt::QueuedConnection);
}

void Job::cancel()
{
    if (d->caps & Cancelable) {
        doCancel();
        if (error() == NoError) {
            setError(Canceled);
            setErrorString(tr("Canceled"));
        }
        emitFinished();
    } else {
        qCDebug(QInstaller::lcInstallerInstallLog) << "The current job cannot be canceled, "
            "missing \"Cancelable\" capability.";
    }
}

quint64 Job::totalAmount() const
{
    return d->totalAmount;
}

quint64 Job::processedAmount() const
{
    return d->processedAmount;
}

void Job::setTotalAmount(quint64 amount)
{
    d->totalAmount = amount;
    emit totalProgress(d->totalAmount);
}

/*!
    Returns the timeout in milliseconds before the job's cancel slot gets triggered. A return value
    of -1 means there is currently no timeout used for the job.
*/
int Job::timeout() const
{
    return d->m_timeout;
}

/*!
    Sets the timeout in \a milliseconds before the job's cancel slot gets triggered. \note Only jobs
    that have the \c Job::Cancelable capability can be canceled by a timeout. A value of -1 will
    stop the timeout mechanism.
*/
void Job::setTimeout(int milliseconds)
{
    d->m_timeout = milliseconds;
}

void Job::setProcessedAmount(quint64 amount)
{
    if (d->processedAmount == amount)
        return;
    d->processedAmount = amount;
    emit progress(this, d->processedAmount, d->totalAmount);
}

void Job::onFinished()
{
    d->m_timer.stop();
    if (d->autoDelete)
        deleteLater();
}

#include "moc_job.cpp"
