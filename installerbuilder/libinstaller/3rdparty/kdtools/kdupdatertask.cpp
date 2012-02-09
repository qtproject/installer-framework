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

#include "kdupdatertask.h"

using namespace KDUpdater;

/*!
   \ingroup kdupdater
   \class KDUpdater::Task kdupdatertask.h KDUpdaterTask
   \brief Base class for all task classes in KDUpdater

   This class is the base class for all task classes in KDUpdater. Task is an activity that
   occupies certain amount of execution time. It can be started, stopped (or canceled), paused and
   resumed. Tasks can report progress and error messages which an application can show in any
   sort of UI. The KDUpdater::Task class provides a common interface for dealing with all kinds of
   tasks in KDUpdater. The class diagram show in this class documentation will help in pointing out
   the task classes in KDUpdater.

   User should be carefull of these points:
   \li Instances of this class cannot be created. Only instance of the subclasses can be created
   \li Task classes can be started only once.
*/

/*!
   \internal
*/
KDUpdater::Task::Task(const QString &name, int caps, QObject *parent)
    : QObject(parent)
    , m_caps(caps)
    , m_name(name)
    , m_errorCode(0)
    , m_started(false)
    , m_finished(false)
    , m_paused(false)
    , m_stopped(false)
    , m_progressPc(0)
    , m_autoDelete(true)
{
}

/*!
   \internal
*/
Task::~Task()
{}

/*!
   Returns the name of the task.
*/
QString Task::name() const
{
    return m_name;
}

/*!
   Returns the capabilities of the task. It is a combination of one or more
   Capability flags. Defined as follows
   \code
   enum Task::Capability
   {
       NoCapability	= 0,
       Pausable     = 1,
       Stoppable    = 2
   };
   \endcode
*/
int Task::capabilities() const
{
    return m_caps;
}

/*!
   Returns the last reported error code.
*/
int Task::error() const
{
    return m_errorCode;
}

/*!
   Returns the last reported error message text.
*/
QString Task::errorString() const
{
    return m_errorText;
}

/*!
   Returns whether the task has started and is running or not.
*/
bool Task::isRunning() const
{
    return m_started;
}

/*!
   Returns whether the task has finished or not.

   \note Stopped (or canceled) tasks are not finished tasks.
*/
bool Task::isFinished() const
{
    return m_finished;
}

/*!
   Returns whether the task is paused or not.
*/
bool Task::isPaused() const
{
    return m_paused;
}

/*!
   Returns whether the task is stopped or not.

   \note Finished tasks are not stopped classes.
*/
bool Task::isStopped() const
{
    return m_stopped;
}

/*!
   Returns the progress in percentage made by this task.
*/
int  Task::progressPercent() const
{
    return m_progressPc;
}

/*!
   Returns a string that describes the progress made by this task as a string.
*/
QString Task::progressText() const
{
    return m_progressText;
}

/*!
   Starts the task.
*/
void Task::run()
{
    if (m_started) {
        qDebug("Trying to start an already started task");
        return;
    }

    if (m_finished || m_stopped) {
        qDebug("Trying to start a finished or canceled task");
        return;
    }

    m_stopped = false;
    m_finished = false; // for the sake of completeness
    m_started = true;
    emit started();
    reportProgress(0, tr("%1 started").arg(m_name));

    doRun();
}

/*!
   Stops the task, provided the task has \ref Stoppable capability.

   \note Once the task is stopped, it cannot be restarted.
*/
void Task::stop()
{
    if (!(m_caps & Stoppable)) {
        const QString errorMsg = tr("%1 cannot be stopped").arg(m_name);
        reportError(ECannotStopTask, errorMsg);
        return;
    }

    if (!m_started) {
        qDebug("Trying to stop an unstarted task");
        return;
    }

    if(m_finished || m_stopped)
    {
        qDebug("Trying to stop a finished or canceled task");
        return;
    }

    m_stopped = doStop();
    if (!m_stopped) {
        const QString errorMsg = tr("Cannot stop task %1").arg(m_name);
        reportError(ECannotStopTask, errorMsg);
        return;
    }

    m_started = false; // the task is not running
    m_finished = false; // the task is not finished, but was canceled half-way through

    emit stopped();
    if (m_autoDelete)
        deleteLater();
}

/*!
   Paused the task, provided the task has \ref Pausable capability.
*/
void Task::pause()
{
    if (!(m_caps & Pausable)) {
        const QString errorMsg = tr("%1 cannot be paused").arg(m_name);
        reportError(ECannotPauseTask, errorMsg);
        return;
    }

    if (!m_started) {
        qDebug("Trying to pause an unstarted task");
        return;
    }

    if (m_finished || m_stopped) {
        qDebug("Trying to pause a finished or canceled task");
        return;
    }

    m_paused = doPause();

    if (!m_paused) {
        const QString errorMsg = tr("Cannot pause task %1").arg(m_name);
        reportError(ECannotPauseTask, errorMsg);
        return;
    }

    // The task state has to be started, paused but not finished or stopped.
    // We need not set the flags below, but just in case.
    // Perhaps we should do Q_ASSERT() ???
    m_started = true;
    m_finished = false;
    m_stopped = false;

    emit paused();
}

/*!
   Resumes the task if it was paused.
*/
void Task::resume()
{
    if (!m_paused) {
        qDebug("Trying to resume an unpaused task");
        return;
    }

    const bool val = doResume();

    if (!val) {
        const QString errorMsg = tr("Cannot resume task %1").arg(m_name);
        reportError(ECannotResumeTask, errorMsg);
        return;
    }

    // The task state should be started, but not paused, finished or stopped.
    // We need not set the flags below, but just in case.
    // Perhaps we should do Q_ASSERT() ???
    m_started = true;
    m_paused = false;
    m_finished = false;
    m_stopped = false;

    emit resumed();
}

/*!
   \internal
*/
void Task::reportProgress(int percent, const QString &text)
{
    if (m_progressPc == percent)
        return;

    m_progressPc = percent;
    m_progressText = text;
    emit progressValue(m_progressPc);
    emit progressText(m_progressText);
}

/*!
   \internal
*/
void Task::reportError(int errorCode, const QString &errorText)
{
    m_errorCode = errorCode;
    m_errorText = errorText;

    emit error(m_errorCode, m_errorText);
    if (m_autoDelete)
        deleteLater();
}

/*!
   \internal
*/
void Task::reportError(const QString &errorText)
{
    reportError(EUnknown, errorText);
}

/*!
   \internal
*/
void Task::reportDone()
{
    QString msg = tr("%1 done");
    reportProgress(100, msg);

    // State should be finished, but not started, paused or stopped.
    m_finished = true;
    m_started = false;
    m_paused = false;
    m_stopped = false;
    m_errorCode = 0;
    m_errorText.clear();

    emit finished();
    if (m_autoDelete)
        deleteLater();
}

bool Task::autoDelete() const
{
    return m_autoDelete;
}

void Task::setAutoDelete(bool autoDelete)
{
    m_autoDelete = autoDelete;
}

/*!
   \fn virtual bool KDUpdater::Task::doStart() = 0;
*/

/*!
   \fn virtual bool KDUpdater::Task::doStop() = 0;
*/

/*!
   \fn virtual bool KDUpdater::Task::doPause() = 0;
*/

/*!
   \fn virtual bool KDUpdater::Task::doResume() = 0;
*/

/*!
   \signal void KDUpdater::Task::error(int code, const QString& errorText)

   This signal is emitted to notify an error during the execution of this task.
   \param code Error code
   \param errorText A string describing the error.

   Error codes are just integers, there are however built in errors represented
   by the KDUpdater::Error enumeration
   \code
   enum Error
   {
   ECannotStartTask,
   ECannotPauseTask,
   ECannotResumeTask,
   ECannotStopTask,
   EUnknown
   };
   \endcode
*/

/*!
   \signal void KDUpdater::Task::progress(int percent, const QString& progressText)

   This signal is emitted to nofity progress made by the task.

   \param percent Percentage of progress made
   \param progressText A string describing the progress made
*/

/*!
   \signal void KDUpdater::Task::started()

   This signal is emitted when the task has started.
*/

/*!
   \signal void KDUpdater::Task::paused()

   This signal is emitted when the task has paused.
*/

/*!
   \signal void KDUpdater::Task::resumed()

   This signal is emitted when the task has resumed.
*/

/*!
   \signal void KDUpdater::Task::stopped()

   This signal is emitted when the task has stopped (or canceled).
*/

/*!
   \signal void KDUpdater::Task::finished()

   This signal is emitted when the task has finished.
*/
