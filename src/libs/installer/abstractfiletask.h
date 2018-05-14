/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
**************************************************************************/

#ifndef ABSTRACTFILETASK_H
#define ABSTRACTFILETASK_H

#include "abstracttask.h"
#include "installer_global.h"

#include <QObject>
#include <QReadWriteLock>

namespace QInstaller {

namespace TaskRole {
enum TaskRole
{
    Checksum,
    TaskItem,
    SourceFile,
    TargetFile,
    Name,
    ChecksumMismatch,
    UserRole = 1000
};
}

class FileTaskItem : public AbstractTaskData
{
public:
    FileTaskItem() {}
    explicit FileTaskItem(const QString &s)
    {
        insert(TaskRole::SourceFile, s);
    }
    FileTaskItem(const QString &s, const QString &t)
    {
        insert(TaskRole::SourceFile, s);
        insert(TaskRole::TargetFile, t);
    }

    QString source() const { return value(TaskRole::SourceFile).toString(); }
    QString target() const { return value(TaskRole::TargetFile).toString(); }
};

class FileTaskResult : public AbstractTaskData
{
public:
    FileTaskResult() {}
    FileTaskResult(const QString &t, const QByteArray &c, const FileTaskItem &i, bool checksumMismatch)
    {
        insert(TaskRole::Checksum, c);
        insert(TaskRole::TargetFile, t);
        insert(TaskRole::TaskItem, QVariant::fromValue(i));
        insert(TaskRole::ChecksumMismatch, checksumMismatch);
    }

    QString target() const { return value(TaskRole::TargetFile).toString(); }
    QByteArray checkSum() const { return value(TaskRole::Checksum).toByteArray(); }
    FileTaskItem taskItem() const { return value(TaskRole::TaskItem).value<FileTaskItem>(); }
    bool checksumMismatch() const { return value(TaskRole::ChecksumMismatch).toBool(); }
};

class INSTALLER_EXPORT AbstractFileTask : public AbstractTask<FileTaskResult>
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractFileTask)

public:
    AbstractFileTask();
    virtual ~AbstractFileTask() {}

    explicit AbstractFileTask(const QString &source);
    explicit AbstractFileTask(const FileTaskItem &item);
    AbstractFileTask(const QString &source, const QString &target);

    QList<FileTaskItem> taskItems() const;
    void setTaskItem(const FileTaskItem &item);

protected:
    void clearTaskItems();
    void addTaskItem(const FileTaskItem &item);
    void setTaskItems(const QList<FileTaskItem> &items);
    void addTaskItems(const QList<FileTaskItem> &items);

private:
    void registerMetaTypes();

private:
    QList<FileTaskItem> m_items;
    mutable QReadWriteLock m_lock;
};

}   // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::FileTaskItem)
Q_DECLARE_METATYPE(QInstaller::FileTaskResult)
Q_DECLARE_METATYPE(QInstaller::TaskException)

#endif // ABSTRACTFILETASK_H
