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

#include "binarydump.h"
#include "binaryreplace.h"
#include "operationrunner.h"

#include <binarycontent.h>
#include <binaryformatenginehandler.h>
#include <errors.h>
#include <fileio.h>
#include <fileutils.h>
#include <init.h>
#include <utils.h>
#include <loggingutils.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QResource>

#include <iomanip>
#include <iostream>

struct Command
{
    const char* command;
    const char* description;
    qint32 argC;
    const char* arguments;
    const char* argDescription;
} Commands[] = {
    { "dump", "Dumps the binary content that belongs to an installer or maintenance tool into "
        "target directory.", 2, "<binary> <targetdirecory>", "The <binary> containing the data to "
        "dump.\nThe <targetdirectory> to dump the data in."
    },

    { "update", "Updates existing installer or maintenance tool with a new installer base.", 2,
        "<binary> <installerbase>", "The <binary> to update.\nThe <installerbase> to use as update."
    },

    { "operation", "Executes an operation with with a given mode and a list of arguments. ", 2,
        "<binary> <mode,name,args,...>", "The <binary> to run the operation with.\n"
        "<mode,name,args,...> 'mode' can be DO or UNDO. 'name' of the operation. 'args,...' "
        "used to run the operation."
    }
};

#define DESCRITION_LENGTH 60
#define SETW_ALIGNLEFT(X) std::setw(X) << std::setiosflags(std::ios::left)

static int fail(const QString &message)
{
    std::cerr << qPrintable(message) << " See 'devtool --help'." << std::endl;
    return EXIT_FAILURE;
}

static QStringList split(int index, const QString &description)
{
    QStringList result;
    if (description.length() <= DESCRITION_LENGTH)
        return result << description;

    const int lastIndexOf = description.left(index + DESCRITION_LENGTH)
        .lastIndexOf(QLatin1Char(' '));
    result << description.left(lastIndexOf);
    return result + split(lastIndexOf + 1, description.mid(lastIndexOf + 1));
}

// -- main

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QLatin1String("2.0.0"));

    QCommandLineParser parser;
    QCommandLineOption help = parser.addHelpOption();
    QCommandLineOption version = parser.addVersionOption();
    QCommandLineOption verbose(QLatin1String("verbose"), QLatin1String("Verbose mode. Prints out "
        "more information."));
    parser.addOption(verbose);

    parser.parse(app.arguments());
    if (parser.isSet(version)) {
        parser.showVersion();
        return EXIT_SUCCESS;
    }

    if (parser.isSet(help)) {
        const QString command = parser.positionalArguments().value(0);
        if (!command.isEmpty()) {
            for (const auto &c : Commands) {
                if (QLatin1String(c.command) == command) {
                    parser.clearPositionalArguments();
                    parser.addPositionalArgument(QString::fromLatin1("%1 %2").arg(QLatin1String(c
                        .command), QLatin1String(c.arguments)), QLatin1String(c.argDescription));
                    parser.showHelp(EXIT_SUCCESS);
                }
            }
            return fail(QString::fromLatin1("\"%1\" is not a devtool command.").arg(command));
        }

        QString helpText = parser.helpText();
        helpText.insert(helpText.indexOf(QLatin1Char(']')) + 1, QLatin1String(" command <args>"));
        std::cout << qPrintable(helpText) << std::endl;
        std::cout << "Available commands (mutually exclusive):" << std::endl;
        for (const auto &c : Commands) {
            QStringList lines = split(0, QLatin1String(c.description));
            std::cout << SETW_ALIGNLEFT(2) << "  " << SETW_ALIGNLEFT(16) << c.command
                << SETW_ALIGNLEFT(DESCRITION_LENGTH) << qPrintable(lines.takeFirst()) << std::endl;
            foreach (const QString &line, lines) {
                std::cout << SETW_ALIGNLEFT(18) <<  QByteArray(18, ' ').constData()
                    << qPrintable(line) << std::endl;
            }
        }
        std::cout << std::endl << "Use 'devtool --help <command>' to read about a specific command."
            << std::endl;
        return EXIT_SUCCESS;
    }

    QStringList arguments = parser.positionalArguments();
    if (arguments.isEmpty())
        return fail(QLatin1String("Missing command."));

    bool found = false;
    const QString command = arguments.takeFirst();
    for (const auto &c : Commands) {
        if ((found = (QLatin1String(c.command) == command))) {
            if (arguments.count() != c.argC)
                return fail(QString::fromLatin1("%1: wrong argument count.").arg(command));
            break;
        }
    }

    if (!found)
        return fail(QString::fromLatin1("\"%1\" is not a devtool command.").arg(command));

    QInstaller::init();
    QInstaller::LoggingHandler::instance().setVerbose(parser.isSet(verbose));

    QString bundlePath;
    QString path = QFileInfo(arguments.first()).absoluteFilePath();
    if (QInstaller::isInBundle(path, &bundlePath)) {
        path = QDir(bundlePath).filePath(QLatin1String("Contents/Resources/installer.dat"));
    }
#ifndef Q_OS_MACOS
    QFileInfo fi = QFileInfo(path);
    bundlePath = path;
    QString tmp = QDir(fi.path()).filePath(QLatin1String("installer.dat"));
    if (QFileInfo::exists(tmp))
        path = tmp;
#endif

    int result = EXIT_FAILURE;
    QVector<QByteArray> resourceMappings;
    quint64 cookie = QInstaller::BinaryContent::MagicCookie;
    try {
        {
            QFile tmpFile(path);
            QInstaller::openForRead(&tmpFile);

            if (!tmpFile.seek(QInstaller::BinaryContent::findMagicCookie(&tmpFile, cookie) - sizeof(qint64)))
                throw QInstaller::Error(QLatin1String("Cannot seek to read magic marker."));

            QInstaller::BinaryLayout layout;
            layout.magicMarker = QInstaller::retrieveInt64(&tmpFile);

            if (layout.magicMarker == QInstaller::BinaryContent::MagicUninstallerMarker) {
                QFileInfo fileInfo(path);

                QInstaller::isInBundle(fileInfo.absoluteFilePath(), &bundlePath);
                fileInfo.setFile(bundlePath);

                path = fileInfo.absolutePath() + QLatin1Char('/') + fileInfo.baseName() + QLatin1String(".dat");

                tmpFile.close();
                tmpFile.setFileName(path);
                QInstaller::openForRead(&tmpFile);

                cookie = QInstaller::BinaryContent::MagicCookieDat;
            }
            layout = QInstaller::BinaryContent::binaryLayout(&tmpFile, cookie);
            tmpFile.close();

            if (command == QLatin1String("update")) {
                BinaryReplace br(layout);   // To update the binary we do not need any mapping.
                return br.replace(arguments.last(), QFileInfo(arguments.first())
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
                throw QInstaller::Error(QLatin1String("Cannot register in-binary resource."));
            resourceMappings.append(ba);
            if (!isOpen)
                resource->close();
        }

        if (command == QLatin1String("dump")) {
            // To dump the content we do not need the binary format engine.
            BinaryDump bd;
            result = bd.dump(manager, arguments.last());
        } else if (command == QLatin1String("operation")) {
            QInstaller::BinaryFormatEngineHandler::instance()->registerResources(manager
                .collections());    // setup the binary format engine

            OperationRunner runner(magicMarker, operations);
            const QStringList operationArguments = arguments.last().split(QLatin1Char(','));
            if (operationArguments.first() == QLatin1String("DO"))
                result = runner.runOperation(operationArguments.mid(1), OperationRunner::RunMode::Do);
            else if (operationArguments.first() == QLatin1String("UNDO"))
                result = runner.runOperation(operationArguments.mid(1), OperationRunner::RunMode::Undo);
            else
                std::cerr << "Malformed argument: " << qPrintable(operationArguments.last()) << std::endl;

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
