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

#ifndef TASK_H
#define TASK_H

#include "updater.h"

#include <QObject>

namespace KDUpdater {

class KDTOOLS_EXPORT Task : public QObject
{
    Q_OBJECT

public:
    enum Capability
    {
        NoCapability = 0,
        Pausable     = 1,
        Stoppable    = 2
    };

    virtual ~Task();

    QString name() const;
    int capabilities() const;

    int error() const;
    QString errorString() const;

    bool isRunning() const;
    bool isFinished() const;
    bool isPaused() const;
    bool isStopped() const;

    int  progressPercent() const;
    QString progressText() const;

    bool autoDelete() const;
    void setAutoDelete(bool autoDelete);

public Q_SLOTS:
    void run();
    void stop();
    void pause();
    void resume();

Q_SIGNALS:
    void error(int code, const QString &errorText);
    void progressValue(int percent);
    void progressText(const QString &progressText);
    void started();
    void paused();
    void resumed();
    void stopped();
    void finished();

protected:
    explicit Task(const QString &name, int caps = NoCapability, QObject *parent = 0);
    void reportProgress(int percent, const QString &progressText);
    void reportError(int errorCode, const QString &errorText);
    void reportError(const QString &errorText);
    void reportDone();

    // Task interface
    virtual void doRun() = 0;
    virtual bool doStop() = 0;
    virtual bool doPause() = 0;
    virtual bool doResume() = 0;

private:
    int m_caps;
    QString m_name;
    int m_errorCode;
    QString m_errorText;
    bool m_started;
    bool m_finished;
    bool m_paused;
    bool m_stopped;
    int m_progressPc;
    QString m_progressText;
    bool m_autoDelete;
};

} // namespace KDUpdater

#endif // TASK_H
