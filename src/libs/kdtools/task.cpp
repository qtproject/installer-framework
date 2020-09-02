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

#include "task.h"

#include "globals.h"

using namespace KDUpdater;

/*!
   \inmodule kdupdater
   \class KDUpdater::Task
   \brief The Task class is the base class for all tasks in KDUpdater.

   This class is the base class for all task classes in KDUpdater. Task is an activity that
   occupies certain amount of execution time. It can be started, stopped (or canceled), paused and
   resumed. Tasks can report progress and error messages which an application can show in any
   sort of UI. The KDUpdater::Task class provides a common interface for dealing with all kinds of
   tasks in KDUpdater.

   User should be careful of these points:
   \list
        \li Task classes can be started only once.
        \li Instances of this class cannot be created. Only instances of the subclasses can.
    \endlist
*/

/*!
    \enum Task::Capability
    This enum value sets the capabilities of the task.

    \value NoCapability
           The task has no capabilities, so it cannot be paused or stopped.
    \value Pausable
           The task can be paused.
    \value Stoppable
           The task can be stopped.
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
   Task::Capability flags.
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
   Returns whether the task has started and is running.
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
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to start an already started task";
        return;
    }

    if (m_stopped) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to start a finished or canceled task";
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
   Stops the task, provided the task has the Task::Stoppable capability.

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
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to stop an unstarted task";
        return;
    }

    if(m_finished || m_stopped)
    {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to stop a finished or canceled task";
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
   Pauses the task, provided the task has the Task::Pausable capability.
*/
void Task::pause()
{
    if (!(m_caps & Pausable)) {
        const QString errorMsg = tr("%1 cannot be paused").arg(m_name);
        reportError(ECannotPauseTask, errorMsg);
        return;
    }

    if (!m_started) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to pause an unstarted task";
        return;
    }

    if (m_finished || m_stopped) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to pause a finished or canceled task";
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
        qCDebug(QInstaller::lcInstallerInstallLog) << "Trying to resume an unpaused task";
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

/*!
    Returns \c true if the task will be automatically deleted.
*/
bool Task::autoDelete() const
{
    return m_autoDelete;
}

/*!
    Automatically deletes the task if \a autoDelete is \c true.
*/
void Task::setAutoDelete(bool autoDelete)
{
    m_autoDelete = autoDelete;
}

/*!
   \fn virtual void KDUpdater::Task::doRun() = 0;

   Returns \c 0 if the task is run.
*/

/*!
   \fn virtual bool KDUpdater::Task::doStop() = 0;

   Returns \c true if the task is stopped.
*/

/*!
   \fn virtual bool KDUpdater::Task::doPause() = 0;

   Returns \c true if the task is paused.
*/

/*!
   \fn virtual bool KDUpdater::Task::doResume() = 0;

   Returns \c true if the task is resumed.
*/

/*!
   \fn void KDUpdater::Task::error(int code, const QString &errorText)

   This signal is emitted to notify an error during the execution of this task.

   The \a code parameter indicates the error that was found during the execution of the
   task, while the \a errorText is the human-readable description of the last error that occurred.
*/

/*!
    \fn void KDUpdater::Task::progressValue(int percent)

    This signal is emitted to report progress made by the task. The \a percent parameter gives
    the progress made as a percentage.
*/

/*!
    \fn void KDUpdater::Task::progressText(const QString &progressText)

    This signal is emitted to report the progress made by the task. The \a progressText parameter
    represents the progress made in a human-readable form.
*/

/*!
   \fn void KDUpdater::Task::started()

   This signal is emitted when the task has started.
*/

/*!
   \fn void KDUpdater::Task::paused()

   This signal is emitted when the task has paused.
*/

/*!
   \fn void KDUpdater::Task::resumed()

   This signal is emitted when the task has resumed.
*/

/*!
   \fn void KDUpdater::Task::stopped()

   This signal is emitted when the task has stopped (or canceled).
*/

/*!
   \fn void KDUpdater::Task::finished()

   This signal is emitted when the task has finished.
*/
