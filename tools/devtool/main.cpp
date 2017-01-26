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

#include "binarydump.h"
#include "binaryreplace.h"
#include "operationrunner.h"

#include <binarycontent.h>
#include <binaryformat.h>
#include <binaryformatenginehandler.h>
#include <errors.h>
#include <fileio.h>
#include <fileutils.h>
#include <init.h>
#include <utils.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QResource>

#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QLatin1String("1.0.0"));

    QCommandLineOption verbose(QLatin1String("verbose"),
        QLatin1String("Verbose mode. Prints out more information."));
    QCommandLineOption dump(QLatin1String("dump"),
        QLatin1String("Dumps the binary content that belongs to an installer or maintenance tool "
        "into target."), QLatin1String("folder"));
    QCommandLineOption run(QLatin1String("operation"),
        QLatin1String("Executes an operation with a list of arguments. Mode can be DO or UNDO."),
        QLatin1String("mode,name,args,..."));
    QCommandLineOption update(QLatin1String("update"),
        QLatin1String("Updates existing installer or maintenance tool with a new installer base."),
        QLatin1String("file"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(verbose);
    parser.addOption(update);
    parser.addOption(dump);
    parser.addOption(run);
    parser.addPositionalArgument(QLatin1String("binary"), QLatin1String("Existing installer or "
        "maintenance tool."));

    parser.process(app.arguments());
    const QStringList arguments = parser.positionalArguments();
    if (arguments.isEmpty() || (arguments.count() > 1))
        parser.showHelp(EXIT_FAILURE);

    QInstaller::init();
    QInstaller::setVerbose(parser.isSet(verbose));

    QString bundlePath;
    QString path = QFileInfo(arguments.first()).absoluteFilePath();
    if (QInstaller::isInBundle(path, &bundlePath))
        path = QDir(bundlePath).filePath(QLatin1String("Contents/Resources/installer.dat"));

    int result = EXIT_FAILURE;
    QVector<QByteArray> resourceMappings;
    quint64 cookie = QInstaller::BinaryContent::MagicCookie;
    try {
        {
            QFile tmp(path);
            QInstaller::openForRead(&tmp);

            if (!tmp.seek(QInstaller::BinaryContent::findMagicCookie(&tmp, cookie) - sizeof(qint64)))
                throw QInstaller::Error(QLatin1String("Could not seek to read magic marker."));

            QInstaller::BinaryLayout layout;
            layout.magicMarker = QInstaller::retrieveInt64(&tmp);

            if (layout.magicMarker == QInstaller::BinaryContent::MagicUninstallerMarker) {
                QFileInfo fi(path);
                if (QInstaller::isInBundle(fi.absoluteFilePath(), &bundlePath))
                    fi.setFile(bundlePath);
                path = fi.absolutePath() + QLatin1Char('/') + fi.baseName() + QLatin1String(".dat");

                tmp.close();
                tmp.setFileName(path);
                QInstaller::openForRead(&tmp);

                cookie = QInstaller::BinaryContent::MagicCookieDat;
            }
            layout = QInstaller::BinaryContent::binaryLayout(&tmp, cookie);
            tmp.close();

            if (parser.isSet(update)) {
                BinaryReplace br(layout);   // To update the binary we do not need any mapping.
                return br.replace(parser.value(update), QFileInfo(arguments.first())
                    .absoluteFilePath());
            }
        }

        QFile file(path);
        QInstaller::openForRead(&file);

        qint64 magicMarker;
        QList<QInstaller::OperationBlob> operations;
        QInstaller::ResourceCollectionManager manager;
        QInstaller::BinaryContent::readBinaryContent(&file, &operations, &manager, &magicMarker,
            cookie);

        // map the inbuilt resources
        const QInstaller::ResourceCollection meta = manager.collectionByName("QResources");
        foreach (const QSharedPointer<QInstaller::Resource> &resource, meta.resources()) {
            const bool isOpen = resource->isOpen();
            if ((!isOpen) && (!resource->open()))
                continue;   // TODO: should we throw here?

            const QByteArray ba = resource->readAll();
            if (!QResource::registerResource((const uchar*) ba.data(), QLatin1String(":/metadata")))
                throw QInstaller::Error(QLatin1String("Could not register in-binary resource."));
            resourceMappings.append(ba);
            if (!isOpen)
                resource->close();
        }

        if (parser.isSet(dump)) {
            // To dump the content we do not need the binary format engine.
            BinaryDump bd;
            result = bd.dump(manager, parser.value(dump));
        } else if (parser.isSet(run)) {
            QInstaller::BinaryFormatEngineHandler::instance()->registerResources(manager
                .collections());    // setup the binary format engine

            OperationRunner runner(magicMarker, operations);
            const QStringList arguments = parser.value(run).split(QLatin1Char(','));
            if (arguments.first() == QLatin1String("DO"))
                result = runner.runOperation(arguments.mid(1), OperationRunner::RunMode::Do);
            else if (arguments.first() == QLatin1String("UNDO"))
                result = runner.runOperation(arguments.mid(1), OperationRunner::RunMode::Undo);
            else
                std::cerr << "Malformed argument: " << qPrintable(parser.value(run)) << std::endl;
        }
    } catch (const QInstaller::Error &error) {
        std::cerr << qPrintable(error.message()) << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
    }

    // unmap resources
    foreach (const QByteArray &rccData, resourceMappings)
        QResource::unregisterResource((const uchar*) rccData.data(), QLatin1String(":/metadata"));

    return result;
}
