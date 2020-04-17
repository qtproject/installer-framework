/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include <packagemanagercore.h>
#include <binarycontent.h>
#include <settings.h>
#include <fileutils.h>
#include <init.h>

#include <QObject>
#include <QFile>
#include <QTest>

using namespace KDUpdater;
using namespace QInstaller;

class tst_licenseagreement : public QObject
{
    Q_OBJECT

private slots:
    void testAutoAcceptFromCLI()
    {
        QInstaller::init(); //This will eat debug output
        PackageManagerCore *core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, QList<OperationBlob> ());
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(":///data/repository");
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);

        QString installDir = QInstaller::generateTemporaryFileName();
        QDir().mkpath(installDir);
        core->setValue(scTargetDir, installDir);
        core->setAutoAcceptLicenses();
        core->installDefaultComponentsSilently();

        QFile file(installDir + "/Licenses/gpl3.txt");
        QVERIFY(file.exists());
        QDir dir(installDir);
        QVERIFY(dir.removeRecursively());
        core->deleteLater();
    }
};

QTEST_MAIN(tst_licenseagreement)

#include "tst_licenseagreement.moc"
