/**************************************************************************
**
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
**************************************************************************/
#include "../shared/packagemanager.h"
#include "../shared/verifyinstaller.h"

#include <component.h>
#include <packagemanagercore.h>

#include <QLoggingCategory>
#include <QTest>
#include <QMessageBox>

using namespace QInstaller;

typedef QList<QPair<QString, QString> > ComponentResourceHash;
typedef QPair<QString, QString> ComponentResource;

static QStringList expectedMessages;

void downloadingArchiveOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type)
    Q_UNUSED(context)
    QByteArray localMsg = msg.toLocal8Bit();
    if (!msg.startsWith("Downloading archive"))
        return;
    if (expectedMessages.contains(msg))
        expectedMessages.removeOne(msg);
}

class tst_ContentSha1Check : public QObject
{
    Q_OBJECT

private slots:

    void testInstall_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<ComponentResourceHash>("componentResources");
        QTest::addColumn<QStringList >("installedFiles");
        QTest::addColumn<QStringList >("expectedDownloadingArchiveMessages");

        /*********** Install with checksum check **********/
        ComponentResourceHash componentResources;
        componentResources.append(ComponentResource("A", "1.0.2-1content.txt"));
        componentResources.append(ComponentResource("B", "1.0.0-1content.txt"));

        QTest::newRow("Check checksum")
                << ":///data/repositorywithchecksumcheck"
                << (QStringList() << "A" << "B")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() << "components.xml" << "A.txt" << "B.txt")
                << (QStringList() << "Downloading archive \"1.0.2-1content.7z.sha1\" for component A."
                                  << "Downloading archive \"1.0.2-1content.7z\" for component A."
                                  << "Downloading archive \"1.0.0-1content.7z.sha1\" for component B."
                                  << "Downloading archive \"1.0.0-1content.7z\" for component B.");

        /*********** Install with and without checksum check **********/
        componentResources.clear();
        componentResources.append(ComponentResource("C", "1.0.2-1content.txt"));
        componentResources.append(ComponentResource("D", "1.0.0-1content.txt"));

        QTest::newRow("Without checksum check")
                << ":///data/repositorywithnochecksumcheck"
                << (QStringList() << "C" << "D")
                << PackageManagerCore::Success
                << componentResources
                << (QStringList() << "components.xml" << "C.txt" << "D.txt")
                << (QStringList() << "Downloading archive \"1.0.2-1content.7z\" for component C."
                                  << "Downloading archive \"1.0.0-1content.7z\" for component D.");

    }

    void testInstallWithInvalidChecksum_data()
    {
        QTest::addColumn<QString>("repository");
        QTest::addColumn<QStringList>("installComponents");
        QTest::addColumn<PackageManagerCore::Status>("status");
        QTest::addColumn<ComponentResourceHash>("componentResources");
        QTest::addColumn<QStringList >("installedFiles");

        /*********** Install with checksum check **********/
        ComponentResourceHash componentResources;

        QTest::newRow("Invalid checksum")
                << ":///data/repositorywithinvalidchecksum"
                << (QStringList() << "E" << "F")
                << PackageManagerCore::Failure
                << componentResources
                << (QStringList());
    }

    void testInstall()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(ComponentResourceHash, componentResources);
        QFETCH(QStringList, installedFiles);
        QFETCH(QStringList, expectedDownloadingArchiveMessages);

        expectedMessages = expectedDownloadingArchiveMessages;
        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, repository));
        qInstallMessageHandler(downloadingArchiveOutput);

        QCOMPARE(status, core->installSelectedComponentsSilently(QStringList() << installComponents));
        for (const ComponentResource &resource : componentResources)
            VerifyInstaller::verifyInstallerResources(m_installDir, resource.first, resource.second);
        VerifyInstaller::verifyFileExistence(m_installDir, installedFiles);

        QVERIFY(expectedMessages.isEmpty());
    }

    void testInstallWithInvalidChecksum()
    {
        QFETCH(QString, repository);
        QFETCH(QStringList, installComponents);
        QFETCH(PackageManagerCore::Status, status);
        QFETCH(ComponentResourceHash, componentResources);
        QFETCH(QStringList, installedFiles);

        QScopedPointer<PackageManagerCore> core(PackageManager::getPackageManagerWithInit
                (m_installDir, repository));
        core->setMessageBoxAutomaticAnswer("DownloadError", QMessageBox::Cancel);
        core->setMessageBoxAutomaticAnswer("installationError", QMessageBox::Ok);

        QCOMPARE(status, core->installSelectedComponentsSilently(QStringList() << installComponents));
        QVERIFY(!QDir().exists(m_installDir));
    }

    void init()
    {
        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
    }

    void cleanup()
    {
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    QString m_installDir;
    QStringList m_expectedMessages;
};


QTEST_MAIN(tst_ContentSha1Check)

#include "tst_contentsha1check.moc"
