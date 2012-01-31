/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/

#include "extractarchiveoperationtest.h"
#include "extractarchiveoperation.h"

#include "init.h"

#include <kdupdaterapplication.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStack>

ExtractArchiveOperationTest::ExtractArchiveOperationTest()
{
    QInstaller::init();
}

void ExtractArchiveOperationTest::init(const QString &outdir)
{
    if (QDir(outdir).exists()) {
        QFAIL("output directory already exists!");
        QVERIFY(false);
    }
    QDir cd(QDir::current());
    QVERIFY(cd.mkdir(outdir));
}

static bool recursiveRemove(const QString &path, QString *errorMsg)
{
    if (errorMsg)
        errorMsg->clear();
    if (!QFileInfo(path).exists())
        return true;
    bool error = false;
    QString msg;
    //first, delete all non-dir files
    QDirIterator it(path, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString n = it.next();
        if (!QFileInfo(n).isDir()) {
            QFile file(n);
            if (!file.remove()) {
                error = true;
                msg = file.errorString();
            }
        }
    }

    QStack<QString> dirs;
    QDirIterator it2(path, QDirIterator::Subdirectories);
    while (it2.hasNext()) {
        const QString n = it2.next();
        if (!n.endsWith(QLatin1String( "/." ) ) && !n.endsWith( QLatin1String( "/.." )))
            dirs.push(n);
    }
    while (!dirs.isEmpty()) {
        const QString n = dirs.top();
        dirs.pop();
        if (!QDir(n).rmdir(QDir::currentPath() + QLatin1String("/") + n)) {
            error = true;
            msg = QObject::tr("Could not remove folder %1").arg(n);
            qDebug() << msg;
        }
    }

    if (!QDir(path).rmdir(QDir::currentPath() + QLatin1String("/") + path)) {
        error = true;
        msg = QObject::tr("Could not remove folder %1: Unknown error").arg(path);
    }

    if (errorMsg)
        *errorMsg = msg;
    return !error;
}

void ExtractArchiveOperationTest::cleanup(const QString &dir)
{
    QDir d(dir);
    QString msg;
    const bool removed = recursiveRemove(dir, &msg);
    if (!removed)
        qCritical() << msg;
    QVERIFY(removed);
}

void ExtractArchiveOperationTest::testExtraction()
{
    const QString outdir = QLatin1String("test-extract-out" );
    init(outdir);
    KDUpdater::Application app;
    QInstaller::ExtractArchiveOperation op;
    op.setArguments(QStringList() << QLatin1String("qt-bin-test.7z") << outdir);
    const bool ok = op.performOperation();
    if (!ok) {
        qCritical() << "Extraction failed:" << op.errorString();
        QFAIL("Extraction failed");
    }
    cleanup(outdir);
}

void ExtractArchiveOperationTest::testExtractionErrors()
{
    const QString outdir = QLatin1String("test-extract-out");
    init(outdir);
    KDUpdater::Application app;
    QInstaller::ExtractArchiveOperation op;
    op.setArguments(QStringList() << QLatin1String("qt-bin-test.7z") << outdir);
    const bool ok = op.performOperation();
    if (!ok) {
        qCritical() << "Extraction failed:" << op.errorString();
        QFAIL("Extraction failed");
    }
    cleanup(outdir);

}

void ExtractArchiveOperationTest::testInvalidArchive()
{
    const QString outdir = QLatin1String("test-extract-out");
    init(outdir);
    KDUpdater::Application app;
    QInstaller::ExtractArchiveOperation op;
    op.setArguments(QStringList() << QLatin1String("test-noarchive.7z") << outdir);
    const bool ok = op.performOperation();
    if (ok) {
        qCritical() << "ExtractArchiveOperation does not report error on extracting invalid archive";
        QFAIL("Extraction failed");
    }
    QVERIFY(op.error() != QInstaller::ExtractArchiveOperation::NoError);
    const QString str = op.errorString();
    qDebug() << str;
    QVERIFY(!str.isEmpty());
    cleanup(outdir);
}

QTEST_MAIN(ExtractArchiveOperationTest)
