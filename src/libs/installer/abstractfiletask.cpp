/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
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
    \enum TaskRole::TaskRole

    \value Checksum
    \value TaskItem
    \value SourceFile
    \value TargetFile
    \value UserRole     The first role that can be used for user-specific purposes.
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
