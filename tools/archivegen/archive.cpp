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

#include <errors.h>
#include <archivefactory.h>
#include <utils.h>

#ifdef IFW_LIB7Z
#include <lib7z_facade.h>
#endif

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMetaEnum>

#include <iostream>

using namespace QInstaller;

int main(int argc, char *argv[])
{
    try {
        QCoreApplication app(argc, argv);
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
        QCoreApplication::setApplicationVersion(QLatin1String(QUOTE(IFW_VERSION_STR)));
#undef QUOTE
#undef QUOTE_
        const QString archiveFormats = ArchiveFactory::supportedTypes().join(QLatin1Char('|'));
        QCommandLineParser parser;
        const QCommandLineOption help = parser.addHelpOption();
        const QCommandLineOption version = parser.addVersionOption();
        const QCommandLineOption format = QCommandLineOption(QStringList()
            << QLatin1String("f") << QLatin1String("format"),
            QCoreApplication::translate("archivegen",
                "%1\n"
                "Format for the archive. Defaults to 7z."
            ).arg(archiveFormats), QLatin1String("format"), QLatin1String("7z"));
        const QCommandLineOption compression = QCommandLineOption(QStringList()
            << QLatin1String("c") << QLatin1String("compression"),
            QCoreApplication::translate("archivegen",
                "0 (No compression)\n"
                "1 (Fastest compressing)\n"
                "3 (Fast compressing)\n"
                "5 (Normal compressing)\n"
                "7 (Maximum compressing)\n"
                "9 (Ultra compressing)\n"
                "Defaults to 5 (Normal compression).\n"
                "Note: some formats do not support all the possible values, "
                "for example bzip2 compression only supports values from 1 to 9."
            ), QLatin1String("5"), QLatin1String("5"));

        parser.addOption(format);
        parser.addOption(compression);
        parser.addPositionalArgument(QLatin1String("archive"),
            QCoreApplication::translate("archivegen", "Compressed archive to create."));
        parser.addPositionalArgument(QLatin1String("sources"),
            QCoreApplication::translate("archivegen", "List of files and directories to compress."));

        parser.parse(app.arguments());
        if (parser.isSet(help)) {
            std::cout << parser.helpText() << std::endl;
            return EXIT_SUCCESS;
        }

        if (parser.isSet(version)) {
            parser.showVersion();
            return EXIT_SUCCESS;
        }

        const QStringList args = parser.positionalArguments();
        if (args.count() < 2) {
            std::cerr << QCoreApplication::translate("archivegen", "Wrong argument count. See "
                "'archivgen --help'.") << std::endl;
            return EXIT_FAILURE;
        }

        bool ok = false;
        QMetaEnum levels = QMetaEnum::fromType<AbstractArchive::CompressionLevel>();
        const int value = parser.value(compression).toInt(&ok);
        if (!ok || !levels.valueToKey(value)) {
            throw QInstaller::Error(QCoreApplication::translate("archivegen",
                "Unknown compression level \"%1\". See 'archivgen --help'.").arg(value));
        }
#ifdef IFW_LIB7Z
        Lib7z::initSevenZ();
#endif
        QString archiveFilename = args[0];
        // Check if filename already has a supported suffix
        if (!ArchiveFactory::isSupportedType(archiveFilename))
             archiveFilename += QLatin1Char('.') + parser.value(format);

        QScopedPointer<AbstractArchive> archive(ArchiveFactory::instance().create(archiveFilename));
        if (!archive) {
            throw QInstaller::Error(QString::fromLatin1("Could not create handler "
                "object for archive \"%1\": \"%2\".").arg(archiveFilename, QLatin1String(Q_FUNC_INFO)));
        }
        archive->setCompressionLevel(AbstractArchive::CompressionLevel(value));
        if (archive->open(QIODevice::WriteOnly) && archive->create(args.mid(1)))
            return EXIT_SUCCESS;

        std::cerr << archive->errorString() << std::endl;
    } catch (const QInstaller::Error &e) {
        std::cerr << e.message() << std::endl;
    } catch (...) {
        std::cerr << QCoreApplication::translate("archivegen", "Unknown exception caught.")
            << std::endl;
    }
    return EXIT_FAILURE;
}
