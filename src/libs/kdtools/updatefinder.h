/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2023 The Qt Company Ltd.
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

#ifndef UPDATEFINDER_H
#define UPDATEFINDER_H

#include "task.h"
#include "packagesource.h"
#include "filedownloader.h"
#include "updatesinfo_p.h"
#include "abstracttask.h"

#include <memory>

using namespace QInstaller;
namespace KDUpdater {

class LocalPackageHub;
class Update;

class ParseXmlFilesTask : public AbstractTask<void>
{
    Q_OBJECT
    Q_DISABLE_COPY(ParseXmlFilesTask)

public:
    ParseXmlFilesTask(UpdatesInfo *info)
        : m_info(info)
    {}

    void doTask(QFutureInterface<void> &fi) override
    {
        fi.reportStarted();
        fi.setExpectedResultCount(1);

        if (fi.isCanceled()) {
            fi.reportFinished();
            return; // ignore already canceled
        }
        m_info->parseFile();

        fi.reportFinished();
    }

private:
    UpdatesInfo *const m_info;
};


class KDTOOLS_EXPORT UpdateFinder : public Task
{
    Q_OBJECT
    class Private;

    struct Data {
        Data()
            : downloader(0) {}
        explicit Data(const QInstaller::PackageSource &i, KDUpdater::FileDownloader *d = 0)
            : info(i), downloader(d) {}

        QInstaller::PackageSource info;
        KDUpdater::FileDownloader *downloader;
    };

    enum struct Resolution {
        AddPackage,
        KeepExisting,
        RemoveExisting
    };

public:
    UpdateFinder();
    ~UpdateFinder();

    QList<Update *> updates() const;

    void setLocalPackageHub(std::weak_ptr<LocalPackageHub> hub);
    void setPackageSources(const QSet<QInstaller::PackageSource> &sources);

private:
    void doRun() override;
    bool doStop() override;
    bool doPause() override;
    bool doResume() override;
    void clear();
    void computeUpdates();
    void cancelComputeUpdates();
    bool downloadUpdateXMLFiles();
    bool parseUpdateXMLFiles();
    bool removeInvalidObjects();
    bool computeApplicableUpdates();

    QList<UpdateInfo> applicableUpdates(UpdatesInfo *updatesInfo);
    void createUpdateObjects(const PackageSource &source, const QList<UpdateInfo> &updateInfoList);
    Resolution checkPriorityAndVersion(const QInstaller::PackageSource &source, const QVariantHash &data) const;
    bool waitForJobToFinish(const int &currentCount, const int &totalsCount);

private slots:
    void parseUpdatesXmlTaskFinished();
    void slotDownloadDone();

private:
    QSet<PackageSource> m_packageSources;
    std::weak_ptr<LocalPackageHub> m_localPackageHub;
    QHash<QString, Update *> m_updates;

    bool m_cancel;
    int m_downloadCompleteCount;
    int m_downloadsToComplete;
    QHash<UpdatesInfo *, Data> m_updatesInfoList;
    int m_updatesXmlTasks;
    int m_updatesXmlTasksToComplete;
    QList<ParseXmlFilesTask*> m_xmlFileTasks;
};

} // namespace KDUpdater

#endif // UPDATEFINDER_H
