/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "binarydump.h"

#include <copydirectoryoperation.h>
#include <errors.h>
#include <fileio.h>

#include <QDirIterator>
#include <QDomDocument>

#include <iostream>

int BinaryDump::dump(const QInstallerCreator::ComponentIndex &index, const QString &target)
{
    QDir targetDir(QFileInfo(target).absoluteFilePath());
    if (targetDir.exists()) {
        if (!targetDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
            std::cerr << qPrintable(QString::fromLatin1("Target directory '%1' already exists and "
                "is not empty.").arg(targetDir.path())) << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        if (!QDir().mkpath(targetDir.path())) {
             std::cerr << qPrintable(QString::fromLatin1("Could not create '%1'.").arg(targetDir
                 .path())) << std::endl;
            return EXIT_FAILURE;
        }
    }

    QInstaller::CopyDirectoryOperation copyMetadata;
    copyMetadata.setArguments(QStringList() << QLatin1String(":/")
        << (targetDir.path() + QLatin1Char('/'))); // Add "/" at the end to make operation work.
    if (!copyMetadata.performOperation()) {
        std::cerr << qPrintable(copyMetadata.errorString()) << std::endl;
        return EXIT_FAILURE;
    }

    if (!targetDir.cd(QLatin1String("metadata"))) {
        std::cerr << qPrintable(QString::fromLatin1("Could not switch to '%1/metadata'.")
            .arg(targetDir.path())) << std::endl;
        return EXIT_FAILURE;
    }

    int result = EXIT_FAILURE;
    try {
        QFile updatesXml(targetDir.filePath(QLatin1String("Updates.xml")));
        QInstaller::openForRead(&updatesXml);

        QString error;
        QDomDocument doc;
        if (!doc.setContent(&updatesXml, &error)) {
            throw QInstaller::Error(QString::fromLatin1("Could not read: '%1'. %2").arg(updatesXml
                .fileName(), error));
        }

        QHash<QString, QString> versionMap;
        const QDomElement root = doc.documentElement();
        const QDomNodeList rootChildNodes = root.childNodes();
        for (int i = 0; i < rootChildNodes.count(); ++i) {
            const QDomElement element = rootChildNodes.at(i).toElement();
            if (element.isNull())
                continue;

            QString name, version;
            if (element.tagName() == QLatin1String("PackageUpdate")) {
                const QDomNodeList elementChildNodes = element.childNodes();
                for (int j = 0; j < elementChildNodes.count(); ++j) {
                    const QDomElement e = elementChildNodes.at(j).toElement();
                    if (e.tagName() == QLatin1String("Name"))
                        name = e.text();
                    else if (e.tagName() == QLatin1String("Version"))
                        version = e.text();
                }
                versionMap.insert(name, version);
            }
        }

        QDirIterator it(targetDir, QDirIterator::Subdirectories);
        while (it.hasNext() && !it.next().isEmpty()) {
            if (!it.fileInfo().isDir())
                continue;

            const QString fileName = it.fileName();
            QInstallerCreator::Component c = index.componentByName(fileName.toUtf8());
            if (c.archives().count() <= 0)
                continue;

            typedef QSharedPointer<QInstallerCreator::Archive> Archive;
            QVector<Archive> archives = c.archives();
            foreach (const Archive &archive, archives) {
                if (!archive->open(QIODevice::ReadOnly))
                    continue;   // TODO: should we throw here?

                QFile target(targetDir.filePath(fileName) + QDir::separator()
                    + QString::fromUtf8(archive->name()));
                QInstaller::openForWrite(&target);
                archive->copyData(&target); // copy the 7z files into the target directory
            }
        }
        result = EXIT_SUCCESS;
    } catch (const QInstaller::Error &error) {
        std::cerr << qPrintable(error.message()) << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
    }
    return result;
}
