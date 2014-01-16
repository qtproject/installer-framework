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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "abstractfiletask.h"

namespace QInstaller {

AbstractFileTask::AbstractFileTask()
{
    registerMetaTypes();
}

AbstractFileTask::AbstractFileTask(const QString &source)
{
    registerMetaTypes();
    setTaskItem(FileTaskItem(source));
}

AbstractFileTask::AbstractFileTask(const FileTaskItem &item)
{
    registerMetaTypes();
    setTaskItem(item);
}

AbstractFileTask::AbstractFileTask(const QString &source, const QString &target)
{
    registerMetaTypes();
    setTaskItem(FileTaskItem(source, target));
}

QList<FileTaskItem> AbstractFileTask::taskItems() const
{
    QReadLocker _(&m_lock);
    return m_items;
}

void AbstractFileTask::setTaskItem(const FileTaskItem &item)
{
    clearTaskItems();
    addTaskItem(item);
}


// -- protected

void AbstractFileTask::clearTaskItems()
{
    QWriteLocker _(&m_lock);
    m_items.clear();
}

void AbstractFileTask::addTaskItem(const FileTaskItem &item)
{
    QWriteLocker _(&m_lock);
    m_items.append(item);
}

void AbstractFileTask::setTaskItems(const QList<FileTaskItem> &items)
{
    clearTaskItems();
    addTaskItems(items);
}

void AbstractFileTask::addTaskItems(const QList<FileTaskItem> &items)
{
    QWriteLocker _(&m_lock);
    m_items.append(items);
}


// -- private

void AbstractFileTask::registerMetaTypes()
{
    qRegisterMetaType<QInstaller::FileTaskItem>();
    qRegisterMetaType<QInstaller::FileTaskResult>();
    qRegisterMetaType<QInstaller::FileTaskException>();
}

}   // namespace QInstaller
