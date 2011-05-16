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

#ifndef KD_UPDATER_TASK_H
#define KD_UPDATER_TASK_H

#include "kdupdater.h"
#include <QObject>

namespace KDUpdater
{
    class KDTOOLS_UPDATER_EXPORT Task : public QObject
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
        void error(int code, const QString& errorText);
        void progressValue(int percent);
        void progressText(const QString& progressText);
        void started();
        void paused();
        void resumed();
        void stopped();
        void finished();

    protected:
        explicit Task(const QString& name, int caps=NoCapability, QObject* parent=0);
        void reportProgress(int percent, const QString& progressText);
        void reportError(int errorCode, const QString& errorText);
        void reportDone();

        void reportError(const QString& errorText)
        {
            reportError(EUnknown, errorText);
        }

    protected:
        // Task interface
        virtual void doRun() = 0;
        virtual bool doStop() = 0;
        virtual bool doPause() = 0;
        virtual bool doResume() = 0;

    private:
        struct TaskData;
        TaskData* d;
    };
}

#endif
