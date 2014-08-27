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
#include "binaryreplace.h"
#include "operationrunner.h"

#include <binarycontent.h>
#include <binaryformat.h>
#include <binaryformatenginehandler.h>
#include <errors.h>
#include <fileio.h>
#include <fileutils.h>
#include <init.h>
#include <kdupdaterupdateoperationfactory.h>
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
        QLatin1String("Dumps the binary content attached to an installer into target."),
        QLatin1String("folder"));
    QCommandLineOption run(QLatin1String("operation"),
        QLatin1String("Executes an operation with a list of arguments. Mode can be DO or UNDO."),
        QLatin1String("mode,name,args,..."));
    QCommandLineOption update(QLatin1String("update"),
        QLatin1String("Updates existing installer or maintenance tool with a new installer base."),
        QLatin1String("file"));

    QCommandLineParser parser;
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

            QInstaller::BinaryLayout layout = QInstaller::BinaryContent::binaryLayout(&tmp,
                cookie);

            if (layout.magicMarker == QInstaller::BinaryContent::MagicUninstallerMarker) {
                QFileInfo fi(path);
                if (QInstaller::isInBundle(fi.absoluteFilePath(), &bundlePath))
                    fi.setFile(bundlePath);
                path = fi.absolutePath() + QLatin1Char('/') + fi.baseName() + QLatin1String(".dat");

                tmp.setFileName(path);
                cookie = QInstaller::BinaryContent::MagicCookieDat;
                layout = QInstaller::BinaryContent::binaryLayout(&tmp, cookie);
            }
            tmp.close();

            if (parser.isSet(update)) {
                // To update the binary we do not need any mapping.
                BinaryReplace br(layout);
                return br.replace(parser.value(update), path);
            }
        }

        QSharedPointer<QFile> file(new QFile(path));
        QInstaller::openForRead(file.data());

        qint64 magicMarker;
        QInstaller::ResourceCollection meta;
        QList<QInstaller::OperationBlob> operations;
        QInstaller::ResourceCollectionManager manager;
        QInstaller::BinaryContent::readBinaryContent(file, &meta, &operations, &manager,
            &magicMarker, cookie);

        // map the inbuilt resources
        foreach (const QSharedPointer<QInstaller::Resource> &resource, meta.resources()) {
            const bool opened = resource->open();
            const QByteArray ba = resource->readAll();
            if (!QResource::registerResource((const uchar*) ba.data(), QLatin1String(":/metadata")))
                throw QInstaller::Error(QLatin1String("Could not register in-binary resource."));
            resourceMappings.append(ba);
            if (opened)
                resource->close();
        }

        if (parser.isSet(dump)) {
            // To dump the content we do not need the binary format engine.
            if (magicMarker != QInstaller::BinaryContent::MagicInstallerMarker)
                throw QInstaller::Error(QLatin1String("Source file is not an installer."));
            BinaryDump bd;
            return bd.dump(manager, parser.value(dump));
        }

        QInstaller::BinaryFormatEngineHandler::instance()->registerResources(manager
            .collections());    // setup the binary format engine

        if (parser.isSet(run)) {
            OperationRunner runner(magicMarker, operations);
            const QStringList arguments = parser.values(run);
            if (arguments.first() == QLatin1String("DO"))
                result = runner.runOperation(arguments.mid(1), OperationRunner::RunMode::Do);
            else if (arguments.first() == QLatin1String("UNDO"))
                result = runner.runOperation(arguments.mid(1), OperationRunner::RunMode::Undo);
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
