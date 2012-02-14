/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#ifndef KDTOOLS_KDJOB_H
#define KDTOOLS_KDJOB_H

#include "kdtoolsglobal.h"

#include <QtCore/QObject>

class KDTOOLS_EXPORT KDJob : public QObject
{
    Q_OBJECT
    class Private;

    Q_PROPERTY(int timeout READ timeout WRITE setTimeout)
    Q_PROPERTY(bool autoDelete READ autoDelete WRITE setAutoDelete)

public:
    explicit KDJob(QObject *parent = 0);
    ~KDJob();

    enum Error {
        NoError = 0,
        Canceled = 1,
        UserDefinedError = 128
    };

    enum Capability {
        NoCapabilities = 0x0,
        Cancelable = 0x1
    };

    Q_DECLARE_FLAGS(Capabilities, Capability)

    int error() const;
    QString errorString() const;

    bool autoDelete() const;
    void setAutoDelete(bool autoDelete);

    Capabilities capabilities() const;
    bool hasCapability(Capability c) const;

    void waitForStarted();
    void waitForFinished();

    quint64 totalAmount() const;
    quint64 processedAmount() const;

    int timeout() const;
    void setTimeout(int milliseconds);

public Q_SLOTS:
    void start();
    void cancel();

Q_SIGNALS:
    void started(KDJob *job);
    void finished(KDJob *job);

    void infoMessage(KDJob *job, const QString &message);
    void progress(KDJob *job, quint64 processed, quint64 total);

protected:
    virtual void doStart() = 0;
    virtual void doCancel() = 0;

    void setCapabilities(Capabilities c);

    void setTotalAmount(quint64 amount);
    void setProcessedAmount(quint64 amount);

    void setError(int error);
    void setErrorString(const QString &errorString);

    void emitFinished();
    void emitFinishedWithError(int error, const QString &errorString);

private Q_SLOTS:
    void onFinished();

private:
    Private *d;
    Q_PRIVATE_SLOT(d, void delayedStart())
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KDJob::Capabilities)

#endif // KDTOOLS_KDJOB_H
