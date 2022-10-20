/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#ifndef ABSTRACTTASK_H
#define ABSTRACTTASK_H

#include "runextensions.h"

#include <QException>
#include <QFutureInterface>
#include <QVariant>

namespace QInstaller {

class AbstractTaskData
{
public:
    AbstractTaskData() {}
    virtual ~AbstractTaskData() = 0;

    QVariant value(int role) const { return m_data.value(role); }
    void insert(int key, const QVariant &value) { m_data.insert(key, value); }

private:
    QHash<int, QVariant> m_data;
};
inline AbstractTaskData::~AbstractTaskData() {}

class TaskException : public QException
{
public:
    TaskException() {}
    ~TaskException() throw() {}
    explicit TaskException(const QString &message)
        : m_message(message)
    {}

    void raise() const override { throw *this; }
    QString message() const { return m_message; }
    TaskException *clone() const override { return new TaskException(*this); }

private:
    QString m_message;
};

template <typename T>
class AbstractTask : public QObject
{
    Q_DISABLE_COPY(AbstractTask)

public:
    AbstractTask() {}
    virtual ~AbstractTask() {}

    virtual void doTask(QFutureInterface<T> &futureInterface) = 0;
};

}   // namespace QInstaller

#endif // ABSTRACTTASK_H
