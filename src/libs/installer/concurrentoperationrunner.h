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

#ifndef CONCURRENTOPERATIONRUNNER_H
#define CONCURRENTOPERATIONRUNNER_H

#include "qinstallerglobal.h"

#include <QObject>
#include <QHash>
#include <QFutureWatcher>

namespace QInstaller {

class INSTALLER_EXPORT ConcurrentOperationRunner : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ConcurrentOperationRunner)

public:
    explicit ConcurrentOperationRunner(QObject *parent = nullptr);
    explicit ConcurrentOperationRunner(OperationList *operations,
        const Operation::OperationType type, QObject *parent = nullptr);
    ~ConcurrentOperationRunner();

    void setOperations(OperationList *operations);
    void setType(const Operation::OperationType type);
    void setMaxThreadCount(int count);

    QHash<Operation *, bool> run();

signals:
    void operationStarted(QInstaller::Operation *operation);
    void progressChanged(const int completed, const int total);
    void finished();

public slots:
    void cancel();

private slots:
    void onOperationStarted(QInstaller::Operation *operation);
    void onOperationfinished();

private:
    bool runOperation(Operation *const operation);
    void reset();

private:
    int m_completedOperations;
    int m_totalOperations;

    QHash<Operation *, QFutureWatcher<bool> *> m_operationWatchers;
    QHash<Operation *, bool> m_results;

    OperationList *m_operations;
    Operation::OperationType m_type;

    QThreadPool *const m_threadPool;
};

} // namespace QInstaller

#endif // CONCURRENTOPERATIONRUNNER_H
