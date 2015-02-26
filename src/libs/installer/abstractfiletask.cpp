/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
**************************************************************************/
#include "abstractfiletask.h"

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::AbstractFileTask
    \brief The AbstractFileTask class is the base class of file related tasks.

    The class is not usable as a standalone class but provides common functionality when subclassed.
*/

/*!
    \inmodule QtInstallerFramework
    \namespace TaskRole
    \brief Contains identifiers for tasks.
*/

/*!
    \enum TaskRole::TaskRole

    \value Checksum
    \value TaskItem
    \value SourceFile
    \value TargetFile
    \value UserRole     The first role that can be used for user-specific purposes.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::FileTaskItem
    \brief The FileTaskItem class represents an item in a file task object.
*/

/*!
    \fn FileTaskItem::FileTaskItem()

    Creates a file task item.
*/

/*!
    \fn FileTaskItem::FileTaskItem(const QString &s)

    Creates a file task item using the source specified by \a s.
*/

/*!
    \fn FileTaskItem::FileTaskItem(const QString &s, const QString &t)

    Creates a file task item using the source specified by \a s and target
    specified by \a t.
*/

/*!
    \fn FileTaskItem::source() const

    Returns the source file of the file task item.
*/

/*!
    \fn FileTaskItem::target() const

    Returns the target file of the file task item.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::FileTaskResult
    \brief The FileTaskResult class represents the results of a file task.
*/

/*!
    \fn FileTaskResult::FileTaskResult()

    Creates file task results.
*/

/*!
    \fn FileTaskResult::FileTaskResult(const QString &t, const QByteArray &c, const FileTaskItem &i)

    Creates file task results using the target file specified by \a t, checksum
    specified by \a c, and file task item specified by \a i.
*/

/*!
    \fn FileTaskResult::target() const

    Returns the target file of the task result.
*/

/*!
    \fn FileTaskResult::checkSum() const

    Returns the checksum of the task result.
*/

/*!
    \fn FileTaskResult::taskItem() const

    Returns file task items.
*/

/*!
    Constructs an empty abstract file task object.
*/
AbstractFileTask::AbstractFileTask()
{
    registerMetaTypes();
}

/*!
    \fn AbstractFileTask::~AbstractFileTask()

    Destroys the abstract file task object.
*/

/*!
    Constructs a new abstract file task object with \a source.
*/
AbstractFileTask::AbstractFileTask(const QString &source)
{
    registerMetaTypes();
    setTaskItem(FileTaskItem(source));
}

/*!
    Constructs a new abstract file task object with \a item.
*/
AbstractFileTask::AbstractFileTask(const FileTaskItem &item)
{
    registerMetaTypes();
    setTaskItem(item);
}

/*!
    Constructs a new abstract file task object with \a source and \a target.
*/
AbstractFileTask::AbstractFileTask(const QString &source, const QString &target)
{
    registerMetaTypes();
    setTaskItem(FileTaskItem(source, target));
}

/*!
    Returns a list of file task items this task is working on.
*/
QList<FileTaskItem> AbstractFileTask::taskItems() const
{
    QReadLocker _(&m_lock);
    return m_items;
}

/*!
    Sets a file task \a item this task is working on.
*/
void AbstractFileTask::setTaskItem(const FileTaskItem &item)
{
    clearTaskItems();
    addTaskItem(item);
}


// -- protected

/*!
    \internal
*/
void AbstractFileTask::clearTaskItems()
{
    QWriteLocker _(&m_lock);
    m_items.clear();
}

/*!
    \internal
*/
void AbstractFileTask::addTaskItem(const FileTaskItem &item)
{
    QWriteLocker _(&m_lock);
    m_items.append(item);
}

/*!
    \internal
*/
void AbstractFileTask::setTaskItems(const QList<FileTaskItem> &items)
{
    clearTaskItems();
    addTaskItems(items);
}

/*!
    \internal
*/
void AbstractFileTask::addTaskItems(const QList<FileTaskItem> &items)
{
    QWriteLocker _(&m_lock);
    m_items.append(items);
}


// -- private

/*!
    \internal
*/
void AbstractFileTask::registerMetaTypes()
{
    qRegisterMetaType<QInstaller::FileTaskItem>();
    qRegisterMetaType<QInstaller::FileTaskResult>();
    qRegisterMetaType<QInstaller::TaskException>();
}

}   // namespace QInstaller
