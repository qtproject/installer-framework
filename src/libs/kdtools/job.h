/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JOB_H
#define JOB_H

#include "kdtoolsglobal.h"

#include <QtCore/QObject>

class KDTOOLS_EXPORT Job : public QObject
{
    Q_OBJECT
    class Private;

    Q_PROPERTY(int timeout READ timeout WRITE setTimeout)
    Q_PROPERTY(bool autoDelete READ autoDelete WRITE setAutoDelete)

public:
    explicit Job(QObject *parent = 0);
    ~Job();

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
    void started(Job *job);
    void finished(Job *job);

    void infoMessage(Job *job, const QString &message);
    void progress(Job *job, quint64 processed, quint64 total);

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

Q_DECLARE_OPERATORS_FOR_FLAGS(Job::Capabilities)

#endif // JOB_H
