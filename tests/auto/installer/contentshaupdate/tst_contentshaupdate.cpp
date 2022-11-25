/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "../shared/packagemanager.h"
#include "../shared/verifyinstaller.h"

#include <QTest>

using namespace QInstaller;

class tst_ContentSha1Update : public QObject
{
    Q_OBJECT

private:
    void setRepository(const QString &repository)
    {
        core->reset();
        core->cancelMetaInfoJob(); //Call cancel to reset metadata so that update repositories are fetched

        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);
    }

private slots:
    void initTestCase()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        core = PackageManager::getPackageManagerWithInit(m_installDir);
    }

    void updateWithContentSha1_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QString>("repositoryForUpdate");
        QTest::addColumn<QString>("component");
        QTest::addColumn<QString>("content");
        QTest::addColumn<QString>("updatedContent");
        QTest::addColumn<PackageManagerCore::Status>("expectedStatusAfterInstall");
        QTest::addColumn<PackageManagerCore::Status>("expectedStatusAfterUpdate");

        QTest::newRow("Sha1UpdateForEssential")
            << ":///data/repositoryWithEssential" << ":///data/repositoryUpdateWithEssential"
            << "componentEssential" << "2.0.0content.txt" << "1.0.0content.txt"
            << PackageManagerCore::Success << PackageManagerCore::EssentialUpdated;
        QTest::newRow("ContentSha1Change")
            << ":///data/repository" << ":///data/repositoryUpdate" << "componentA" << "1.0.0content.txt" << "0.1.0content.txt"
            << PackageManagerCore::Success << PackageManagerCore::Success;
        QTest::newRow("NewContentSha1")
            << ":///data/repository" << ":///data/repositoryUpdate" << "componentB" << "1.0.0content.txt" << "0.1.0content.txt"
            << PackageManagerCore::Success << PackageManagerCore::Success;
        QTest::newRow("SameContentSha1")
            << ":///data/repository" << ":///data/repositoryUpdate" << "componentC" << "1.0.0content.txt" << "1.0.0content.txt"
            << PackageManagerCore::Success << PackageManagerCore::Canceled;
        QTest::newRow("Sha1RemovedFromRepo")
            << ":///data/repository" << ":///data/repositoryUpdate" << "componentD" << "1.0.0content.txt" << "2.0.0content.txt"
            << PackageManagerCore::Success << PackageManagerCore::Success;
    }

    void updateWithContentSha1()
    {
        QFETCH(QString, repository);
        QFETCH(QString, repositoryForUpdate);
        QFETCH(QString, component);
        QFETCH(QString, content);
        QFETCH(QString, updatedContent);
        QFETCH(PackageManagerCore::Status, expectedStatusAfterInstall);
        QFETCH(PackageManagerCore::Status, expectedStatusAfterUpdate);

        setRepository(repository);
        QCOMPARE(expectedStatusAfterInstall, core->installSelectedComponentsSilently(QStringList() << component));
        QCOMPARE(expectedStatusAfterInstall, core->status());
        VerifyInstaller::verifyInstallerResources(m_installDir, component, content);

        core->commitSessionOperations();
        core->setPackageManager();
        setRepository(repositoryForUpdate);
        QCOMPARE(expectedStatusAfterUpdate, core->updateComponentsSilently(QStringList()));
        VerifyInstaller::verifyInstallerResources(m_installDir, component, updatedContent);
    }

    void cleanupTestCase()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
        delete core;
    }

private:
    QString m_installDir;
    PackageManagerCore *core;
};


QTEST_MAIN(tst_ContentSha1Update)

#include "tst_contentshaupdate.moc"
