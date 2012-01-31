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
#include <common/binaryformat.h>
#include <common/errors.h>
#include <common/fileutils.h>
#include <common/utils.h>

#include <init.h>

#include <kdsavefile.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QStringList>

#include <iostream>

using namespace QInstaller;
using namespace QInstallerCreator;

void writeMaintenanceBinary(const QString &maintenanceTool, QFile *const input, qint64 size)
{
    KDSaveFile out(maintenanceTool + QLatin1String(".new"));
    openForWrite(&out, out.fileName()); // throws an exception in case of error

    if (!input->seek(0)) {
        throw Error(QObject::tr("Failed to seek in file %1. Reason: %2.").arg(input->fileName(),
            input->errorString()));
    }

    appendData(&out, input, size);
    appendInt64(&out, 0);   // resource count
    appendInt64(&out, 4 * sizeof(qint64));   // data block size
    appendInt64(&out, QInstaller::MagicUninstallerMarker);
    appendInt64(&out, QInstaller::MagicCookie);

    out.setPermissions(out.permissions() | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther
        | QFile::ExeOther | QFile::ExeGroup | QFile::ExeUser);

    if (!out.commit(KDSaveFile::OverwriteExistingFile)) {
        throw Error(QString::fromLatin1("Could not write new maintenance-tool to %1. Reason: %2.").arg(out
            .fileName(), out.errorString()));
    }
}

void writeBinaryDataFile(QIODevice *output, QFile *const input, const OperationList &performedOperations,
    const BinaryLayout &layout)
{
    const qint64 dataBlockStart = output->pos();

    QVector<Range<qint64> >resourceSegments;
    foreach (const Range<qint64> &segment, layout.metadataResourceSegments) {
        input->seek(segment.start());
        resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), segment.length()));
        appendData(output, input, segment.length());
    }

    const qint64 operationsStart = output->pos();
    appendInt64(output, performedOperations.count());
    foreach (Operation *operation, performedOperations) {
        // the installer can't be put into XML, remove it first
        operation->clearValue(QLatin1String("installer"));

        appendString(output, operation->name());
        appendString(output, operation->toXml().toString());

        // for the ui not to get blocked
        qApp->processEvents();
    }
    appendInt64(output, performedOperations.count());
    const qint64 operationsEnd = output->pos();

    // we don't save any component-indexes.
    const qint64 numComponents = 0;
    appendInt64(output, numComponents); // for the indexes
    // we don't save any components.
    const qint64 compIndexStart = output->pos();
    appendInt64(output, numComponents); // and 2 times number of components,
    appendInt64(output, numComponents); // one before and one after the components
    const qint64 compIndexEnd = output->pos();

    appendInt64Range(output, Range<qint64>::fromStartAndEnd(compIndexStart, compIndexEnd)
        .moved(-dataBlockStart));
    foreach (const Range<qint64> segment, resourceSegments)
        appendInt64Range(output, segment.moved(-dataBlockStart));
    appendInt64Range(output, Range<qint64>::fromStartAndEnd(operationsStart, operationsEnd)
        .moved(-dataBlockStart));
    appendInt64(output, layout.resourceCount);
    //data block size, from end of .exe to end of file
    appendInt64(output, output->pos() + 3 * sizeof(qint64) - dataBlockStart);
    appendInt64(output, MagicUninstallerMarker);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QStringList arguments = app.arguments();
    if (arguments.count() < 3) {
        std::cout << "Usage: extractbinarydata [--verbose --extract-binarydata] maintenance-tool "
            "installer-base" << std::endl;
        return EXIT_FAILURE;
    }

    {
        QInstaller::init();
        QInstaller::setVerbose(arguments.contains(QLatin1String("--verbose")));
        arguments.removeAll(QLatin1String("--verbose"));

        try {
            // write out the new maintenance tool
            QFile installerBase(arguments.at(2));
            openForRead(&installerBase, installerBase.fileName());
            writeMaintenanceBinary(arguments.at(1), &installerBase, installerBase.size());
        } catch (const Error &error) {
            qDebug() << error.message();
            return EXIT_FAILURE;
        }

        if (arguments.contains(QLatin1String("--extract-binarydata"))) {
            BinaryLayout layout;
            QFile input(arguments.at(1));
            try {
                openForRead(&input, input.fileName());
                layout = BinaryContent::readBinaryLayout(&input, findMagicCookie(&input, MagicCookie));
            } catch (const Error &error) {
                qDebug() << error.message();
                return EXIT_FAILURE;
            }

            if (layout.magicMarker != MagicUninstallerMarker) {
                std::cout << "Extracting binary data only supported for maintenance tools." << std::endl;
                return EXIT_FAILURE;
            }

            try {
                KDSaveFile file(QFileInfo(arguments.at(1)).absolutePath() + QDir::separator()
                    + QFileInfo(arguments.at(1)).baseName() + QLatin1String(".dat"));
                openForWrite(&file, file.fileName());
                BinaryContent content = BinaryContent::readFromBinary(arguments.at(1));
                writeBinaryDataFile(&file, &input, content.performedOperations(), layout);
                appendInt64(&file, MagicCookieDat);
                file.setPermissions(file.permissions() | QFile::WriteUser | QFile::ReadGroup
                    | QFile::ReadOther);
                if (!file.commit(KDSaveFile::OverwriteExistingFile)) {
                    throw Error(QString::fromLatin1("Couldn't write binary data to %1. Reason: %2").arg(file
                        .fileName(), file.errorString()));
                }
                file.close();
                input.close();
            } catch (const Error &error) {
                qDebug() << error.message();
                return EXIT_FAILURE;
            }
        }
    }

    QFile::rename(arguments.at(1), arguments.at(1) + QLatin1String(".org"));
    QFile::rename(arguments.at(1) + QLatin1String(".new"), arguments.at(1));

    return EXIT_SUCCESS;
}
