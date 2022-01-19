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
#ifndef TESTREPOSITORY_H
#define TESTREPOSITORY_H

#include "downloadfiletask.h"
#include "job.h"
#include "repository.h"

#include <QFutureWatcher>
#include <QTimer>

namespace QInstaller {

class PackageManagerCore;

class INSTALLER_EXPORT TestRepository : public Job
{
    Q_OBJECT
    Q_DISABLE_COPY(TestRepository)

public:
    explicit TestRepository(PackageManagerCore *parent = 0);
    ~TestRepository();

    Repository repository() const;
    void setRepository(const Repository &repository);

private slots:
    void doStart() override;
    void doCancel() override;

    void onTimeout();
    void downloadCompleted();

private:
    void reset();

private:
    PackageManagerCore *m_core;

    QTimer m_timer;
    Repository m_repository;
    QFutureWatcher<FileTaskResult> m_xmlTask;
};

} // namespace QInstaller

#endif  // TESTREPOSITORY_H
