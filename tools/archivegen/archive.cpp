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

#include <errors.h>
#include <lib7z_create.h>
#include <lib7z_facade.h>
#include <utils.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>

#include <iostream>

using namespace QInstaller;

class FailOnErrorCallback : public Lib7z::UpdateCallback
{
    HRESULT OpenFileError(const wchar_t*, DWORD) {
        return S_FALSE;
    }

    HRESULT CanNotFindError(const wchar_t*, DWORD) {
        return S_FALSE;
    }

    HRESULT OpenResult(const wchar_t*, HRESULT result, const wchar_t*) {
        return result;
    }
};

class VerbosePrinterCallback : public Lib7z::UpdateCallback
{
public:
    ~VerbosePrinterCallback() {
        m_PercentPrinter.ClosePrint();
    }

private:
    HRESULT SetTotal(UInt64 size) {
        m_PercentPrinter.SetTotal(size);
        return S_OK;
    }

    HRESULT SetCompleted(const UInt64 *size) {
        if (size) {
            m_PercentPrinter.SetRatio(*size);
            m_PercentPrinter.PrintRatio();
        }
        return S_OK;
    }

    HRESULT OpenResult(const wchar_t *file, HRESULT result, const wchar_t*) {
        if (result != S_OK) {
            printBlock(QCoreApplication::translate("archivegen", "Cannot update file \"%1\". "
                "Unsupported archive.").arg(QDir::toNativeSeparators(QString::fromWCharArray(file))), Q_NULLPTR);
        }
        return result;
    }

    HRESULT OpenFileError(const wchar_t *file, DWORD) {
        printBlock(QCoreApplication::translate("archivegen", "Cannot open file "), file);
        return S_FALSE;
    }

    HRESULT CanNotFindError(const wchar_t *file, DWORD) {
        printBlock(QCoreApplication::translate("archivegen", "Cannot find file "), file);
        return S_FALSE;
    }

    HRESULT StartArchive(const wchar_t *name, bool) {
        printLine(QCoreApplication::translate("archivegen", "Create archive."));
        if (name) {
            m_PercentPrinter.PrintNewLine();
            m_PercentPrinter.PrintString(name);
        }
        return S_OK;
    }

    HRESULT FinishArchive() {
        m_PercentPrinter.PrintNewLine();
        printLine(QCoreApplication::translate("archivegen", "Finished archive."));
        return S_OK;
    }

    void printLine(const QString &message) {
        m_PercentPrinter.PrintString(message.toStdWString().c_str());
    }

    void printBlock(const QString &message, const wchar_t *message2) {
        m_PercentPrinter.PrintNewLine();
        m_PercentPrinter.PrintString(message.toStdWString().c_str());
        if (message2)
            m_PercentPrinter.PrintString(message2);
        m_PercentPrinter.PrintNewLine();
    }

    Lib7z::PercentPrinter m_PercentPrinter;
};

int main(int argc, char *argv[])
{
    try {
        QCoreApplication app(argc, argv);
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
        QCoreApplication::setApplicationVersion(QLatin1String(QUOTE(IFW_VERSION_STR)));
#undef QUOTE
#undef QUOTE_

        QCommandLineParser parser;
        const QCommandLineOption help = parser.addHelpOption();
        const QCommandLineOption version = parser.addVersionOption();
        QCommandLineOption verbose(QLatin1String("verbose"),
            QCoreApplication::translate("archivegen", "Verbose mode. Prints out more information."));
        const QCommandLineOption compression = QCommandLineOption(QStringList()
            << QLatin1String("c") << QLatin1String("compression"),
            QCoreApplication::translate("archivegen",
                "0 (No compression)\n"
                "1 (Fastest compressing)\n"
                "3 (Fast compressing)\n"
                "5 (Normal compressing)\n"
                "7 (Maximum compressing)\n"
                "9 (Ultra compressing)\n"
                "Defaults to 5 (Normal compression)."
            ), QLatin1String("5"), QLatin1String("5"));

        parser.addOption(verbose);
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
        const int values[6] = { 0, 1, 3, 5, 7, 9 };
        const int value = parser.value(compression).toInt(&ok);
        if (!ok || (std::find(std::begin(values), std::end(values), value) == std::end(values))) {
            throw QInstaller::Error(QCoreApplication::translate("archivegen",
                "Unknown compression level \"%1\". See 'archivgen --help'.").arg(value));
        }

        Lib7z::initSevenZ();
        Lib7z::createArchive(args[0], args.mid(1), Lib7z::QTmpFile::No, Lib7z::Compression(value),
            [&] () -> Lib7z::UpdateCallback * {
                if (parser.isSet(verbose))
                    return new VerbosePrinterCallback;
                return new FailOnErrorCallback;
            } ()
        );
        return EXIT_SUCCESS;
    } catch (const QInstaller::Error &e) {
        std::cerr << e.message() << std::endl;
    } catch (...) {
        std::cerr << QCoreApplication::translate("archivegen", "Unknown exception caught.")
            << std::endl;
    }
    return EXIT_FAILURE;
}
