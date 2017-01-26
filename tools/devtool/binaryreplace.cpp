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

#include "binaryreplace.h"

#include <binarycontent.h>
#include <copyfiletask.h>
#include <downloadfiletask.h>
#include <errors.h>
#include <fileio.h>
#include <fileutils.h>
#include <lib7z_facade.h>

#include <QDir>
#include <QFutureWatcher>

#include <iostream>

BinaryReplace::BinaryReplace(const QInstaller::BinaryLayout &layout)
    : m_binaryLayout(layout)
{}

int BinaryReplace::replace(const QString &source, const QString &target)
{
    const QUrl url = QUrl::fromUserInput(source);
    QFutureWatcher<QInstaller::FileTaskResult> taskWatcher;
    if (url.isRelative() || url.isLocalFile()) {
        taskWatcher.setFuture(QtConcurrent::run(&QInstaller::CopyFileTask::doTask,
            new QInstaller::CopyFileTask(QInstaller::FileTaskItem(url.isRelative()
            ? QFileInfo(source).absoluteFilePath() : url.toLocalFile()))));
    } else {
        taskWatcher.setFuture(QtConcurrent::run(&QInstaller::DownloadFileTask::doTask,
            new QInstaller::DownloadFileTask(QInstaller::FileTaskItem(url.toString()))));
    }

    bool result = EXIT_FAILURE;
    try {
        taskWatcher.waitForFinished(); // throws on error
        const QFuture<QInstaller::FileTaskResult> future = taskWatcher.future();
        if (future.resultCount() <= 0)
            return result;

        QString newInstallerBasePath = future.result().target();
        if (Lib7z::isSupportedArchive(newInstallerBasePath)) {
            QFile archive(newInstallerBasePath);
            if (archive.open(QIODevice::ReadOnly)) {
                try {
                    Lib7z::extractArchive(&archive, QDir::tempPath());
                    const QVector<Lib7z::File> files = Lib7z::listArchive(&archive);
                    newInstallerBasePath = QDir::tempPath() + QLatin1Char('/') + files.value(0)
                        .path;
                    result = EXIT_SUCCESS;
                } catch (const Lib7z::SevenZipException& e) {
                    std::cerr << qPrintable(QString::fromLatin1("Error while extracting '%1': %2.")
                        .arg(newInstallerBasePath, e.message())) << std::endl;
                } catch (...) {
                    std::cerr << qPrintable(QString::fromLatin1("Unknown exception caught while "
                        "extracting '%1'.").arg(newInstallerBasePath)) << std::endl;
                }
            } else {
                std::cerr << qPrintable(QString::fromLatin1("Could not open '%1' for reading: %2.")
                    .arg(newInstallerBasePath, archive.errorString())) << std::endl;
            }
            if (!archive.remove()) {
                std::cerr << qPrintable(QString::fromLatin1("Could not delete file '%1': %2.")
                    .arg(newInstallerBasePath, archive.errorString())) << std::endl;
            }
            if (result != EXIT_SUCCESS)
                return result;
        }

        result = EXIT_FAILURE;
        try {
            QFile installerBaseNew(newInstallerBasePath);
#ifndef Q_OS_OSX
            QFile installerBaseOld(target);
            QInstaller::openForAppend(&installerBaseNew);

            installerBaseNew.seek(installerBaseNew.size());
            if (m_binaryLayout.magicMarker == QInstaller::BinaryContent::MagicInstallerMarker) {
                QInstaller::openForRead(&installerBaseOld);
                installerBaseOld.seek(m_binaryLayout.metaResourcesSegment.start());
                QInstaller::appendData(&installerBaseNew, &installerBaseOld, installerBaseOld
                    .size() - installerBaseOld.pos());
                installerBaseOld.close();
            } else {
                QInstaller::appendInt64(&installerBaseNew, 0);
                QInstaller::appendInt64(&installerBaseNew, 4 * sizeof(qint64));
                QInstaller::appendInt64(&installerBaseNew,
                    QInstaller::BinaryContent::MagicUninstallerMarker);
                QInstaller::appendInt64(&installerBaseNew,
                    QInstaller::BinaryContent::MagicCookie);
            }
            installerBaseNew.close();
#else
            QString bundlePath;
            QInstaller::isInBundle(target, &bundlePath);
            QFile installerBaseOld(QDir(bundlePath).filePath(bundlePath
                + QLatin1String("/Contents/MacOS/") + QFileInfo(bundlePath).completeBaseName()));
#endif
            QFile backup(installerBaseOld.fileName() + QLatin1String(".bak"));
            if (backup.exists() && (!backup.remove())) {
                std::cerr << qPrintable(QString::fromLatin1("Could not delete '%1'. %2")
                    .arg(backup.fileName(), backup.errorString())) << std::endl;
            }

            const QString oldBasePath = installerBaseOld.fileName();
            if (!installerBaseOld.rename(oldBasePath + QLatin1String(".bak"))) {
                std::cerr << qPrintable(QString::fromLatin1("Could not rename '%1' to '%2'. %3")
                    .arg(oldBasePath, oldBasePath + QLatin1String(".bak"),
                    installerBaseOld.errorString())) << std::endl;
            }

            if (!installerBaseNew.rename(oldBasePath)) {
                std::cerr << qPrintable(QString::fromLatin1("Could not copy '%1' to '%2'. %3")
                    .arg(installerBaseNew.fileName(), oldBasePath, installerBaseNew.errorString()))
                    << std::endl;
            } else {
                result = EXIT_SUCCESS;
                installerBaseNew.setPermissions(installerBaseOld.permissions());
            }
        } catch (const QInstaller::Error &error) {
            std::cerr << qPrintable(error.message()) << std::endl;
        }
    } catch (const QInstaller::TaskException &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
    } catch (const QUnhandledException &e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught."<< std::endl;
    }
    return result;
}
