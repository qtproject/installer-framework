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

#include "utils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProcessEnvironment>
#include <QThread>
#include <QVector>

#if defined(Q_OS_WIN) || defined(Q_OS_WINCE)
#   include "qt_windows.h"
#endif

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef Q_OS_UNIX
#include <errno.h>
#include <signal.h>
#include <time.h>
#endif

void QInstaller::uiDetachedWait(int ms)
{
    QTime timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
        QThread::msleep(10UL);
    } while (timer.elapsed() < ms);
}

/*!
    Starts the program \a program with the arguments \a arguments in a new process, and detaches
    from it. Returns true on success; otherwise returns false. If the calling process exits, the
    detached process will continue to live.

    Note that arguments that contain spaces are not passed to the process as separate arguments.

    Unix:    The started process will run in its own session and act like a daemon.
    Windows: Arguments that contain spaces are wrapped in quotes. The started process will run as
             a regular standalone process.

    The process will be started in the directory \a workingDirectory.

    If the function is successful then \a *pid is set to the process identifier of the started
    process.

    Additional note: The difference in using this function over its equivalent from QProcess
                     appears on Windows. While this function will truly detach and not show a GUI
                     window for the started process, the QProcess version will.
*/
bool QInstaller::startDetached(const QString &program, const QStringList &arguments,
    const QString &workingDirectory, qint64 *pid)
{
    bool success = false;
#ifdef Q_OS_WIN
    PROCESS_INFORMATION pinfo;
    STARTUPINFOW startupInfo = { sizeof(STARTUPINFO), 0, 0, 0,
        static_cast<ulong>(CW_USEDEFAULT), static_cast<ulong>(CW_USEDEFAULT),
        static_cast<ulong>(CW_USEDEFAULT), static_cast<ulong>(CW_USEDEFAULT),
        0, 0, 0, STARTF_USESHOWWINDOW, SW_HIDE, 0, 0, 0, 0, 0
    };  // That's the difference over QProcess::startDetached(): STARTF_USESHOWWINDOW, SW_HIDE.

    const QString commandline = QInstaller::createCommandline(program, arguments);
    if (CreateProcessW(0, (wchar_t*) commandline.utf16(),
        0, 0, false, CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
        0, workingDirectory.isEmpty() ? 0 : (wchar_t*) workingDirectory.utf16(),
        &startupInfo, &pinfo)) {
        success = true;
        CloseHandle(pinfo.hThread);
        CloseHandle(pinfo.hProcess);
        if (pid)
            *pid = pinfo.dwProcessId;
    }
#else
    success = QProcess::startDetached(program, arguments, workingDirectory, pid);
#endif
    return success;
}

// Returns ["en-us", "en"] for "en-us"
QStringList QInstaller::localeCandidates(const QString &locale_)
{
    QStringList candidates;
    QString locale = locale_;
    candidates.reserve(locale.count(QLatin1Char('-')));
    forever {
        candidates.append(locale);
        int r = locale.lastIndexOf(QLatin1Char('-'));
        if (r <= 0)
            break;
        locale.truncate(r);
    }
    return candidates;
}


static bool verb = false;

void QInstaller::setVerbose(bool v)
{
    verb = v;
}

bool QInstaller::isVerbose()
{
    return verb;
}

std::ostream &QInstaller::operator<<(std::ostream &os, const QString &string)
{
    return os << qPrintable(string);
}

QByteArray QInstaller::calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo)
{
    Q_ASSERT(device);
    QCryptographicHash hash(algo);
    static QByteArray buffer(1024 * 1024, '\0');
    while (true) {
        const qint64 numRead = device->read(buffer.data(), buffer.size());
        if (numRead <= 0)
            return hash.result();
        hash.addData(buffer.constData(), numRead);
    }
    return QByteArray(); // never reached
}

QByteArray QInstaller::calculateHash(const QString &path, QCryptographicHash::Algorithm algo)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return calculateHash(&file, algo);
}

QString QInstaller::replaceVariables(const QHash<QString, QString> &vars, const QString &str)
{
    QString res;
    int pos = 0;
    while (true) {
        int pos1 = str.indexOf(QLatin1Char('@'), pos);
        if (pos1 == -1)
            break;
        int pos2 = str.indexOf(QLatin1Char('@'), pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += vars.value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QString QInstaller::replaceWindowsEnvironmentVariables(const QString &str)
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString res;
    int pos = 0;
    while (true) {
        int pos1 = str.indexOf(QLatin1Char( '%'), pos);
        if (pos1 == -1)
            break;
        int pos2 = str.indexOf(QLatin1Char( '%'), pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += env.value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QInstaller::VerboseWriter::VerboseWriter()
{
    preFileBuffer.open(QIODevice::ReadWrite);
    stream.setDevice(&preFileBuffer);
    currentDateTimeAsString = QDateTime::currentDateTime().toString();
}

QInstaller::VerboseWriter::~VerboseWriter()
{
    stream.flush();
    if (logFileName.isEmpty()) // binarycreator
        return;
    //if the installer installed nothing - there is no target directory - where the logfile can be saved
    if (!QFileInfo(logFileName).absoluteDir().exists())
        return;

    QFile output(logFileName);
    if (output.open(QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text)) {
        QString logInfo;
        logInfo += QLatin1String("************************************* Invoked: ");
        logInfo += currentDateTimeAsString;
        logInfo += QLatin1String("\n");
        output.write(logInfo.toLocal8Bit());
        output.write(preFileBuffer.data());
        output.close();
    }
    stream.setDevice(0);
}

void QInstaller::VerboseWriter::setFileName(const QString &fileName)
{
    logFileName = fileName;
}


Q_GLOBAL_STATIC(QInstaller::VerboseWriter, verboseWriter)

QInstaller::VerboseWriter *QInstaller::VerboseWriter::instance()
{
    return verboseWriter();
}

void QInstaller::VerboseWriter::appendLine(const QString &msg)
{
    stream << msg << endl;
}

#ifdef Q_OS_WIN
// taken from qcoreapplication_p.h
template<typename Char>
static QVector<Char*> qWinCmdLine(Char *cmdParam, int length, int &argc)
{
    QVector<Char*> argv(8);
    Char *p = cmdParam;
    Char *p_end = p + length;

    argc = 0;

    while (*p && p < p_end) {                                // parse cmd line arguments
        while (QChar((short)(*p)).isSpace())                          // skip white space
            p++;
        if (*p && p < p_end) {                                // arg starts
            int quote;
            Char *start, *r;
            if (*p == Char('\"') || *p == Char('\'')) {        // " or ' quote
                quote = *p;
                start = ++p;
            } else {
                quote = 0;
                start = p;
            }
            r = start;
            while (*p && p < p_end) {
                if (quote) {
                    if (*p == quote) {
                        p++;
                        if (QChar((short)(*p)).isSpace())
                            break;
                        quote = 0;
                    }
                }
                if (*p == '\\') {                // escape char?
                    p++;
                    if (*p == Char('\"') || *p == Char('\''))
                        ;                        // yes
                    else
                        p--;                        // treat \ literally
                } else {
                    if (!quote && (*p == Char('\"') || *p == Char('\''))) {        // " or ' quote
                        quote = *p++;
                        continue;
                    } else if (QChar((short)(*p)).isSpace() && !quote)
                        break;
                }
                if (*p)
                    *r++ = *p++;
            }
            if (*p && p < p_end)
                p++;
            *r = Char('\0');

            if (argc >= (int)argv.size()-1)        // expand array
                argv.resize(argv.size()*2);
            argv[argc++] = start;
        }
    }
    argv[argc] = 0;

    return argv;
}

QStringList QInstaller::parseCommandLineArgs(int argc, char **argv)
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    QStringList arguments;
    QString cmdLine = QString::fromWCharArray(GetCommandLine());

    QVector<wchar_t*> args = qWinCmdLine<wchar_t>((wchar_t *)cmdLine.utf16(), cmdLine.length(), argc);
    for (int a = 0; a < argc; ++a)
        arguments << QString::fromWCharArray(args[a]);
    return arguments;
}
#else
QStringList QInstaller::parseCommandLineArgs(int argc, char **argv)
{
    QStringList arguments;
    for (int a = 0; a < argc; ++a)
        arguments << QString::fromLocal8Bit(argv[a]);
    return arguments;
}
#endif

#ifdef Q_OS_WIN
// taken from qprocess_win.cpp
static QString qt_create_commandline(const QString &program, const QStringList &arguments)
{
    QString args;
    if (!program.isEmpty()) {
        QString programName = program;
        if (!programName.startsWith(QLatin1Char('\"')) && !programName.endsWith(QLatin1Char('\"'))
            && programName.contains(QLatin1Char(' '))) {
                programName = QLatin1Char('\"') + programName + QLatin1Char('\"');
        }
        programName.replace(QLatin1Char('/'), QLatin1Char('\\'));

        // add the program as the first arg ... it works better
        args = programName + QLatin1Char(' ');
    }

    for (int i = 0; i < arguments.size(); ++i) {
        QString tmp = arguments.at(i);
        // in the case of \" already being in the string the \ must also be escaped
        tmp.replace(QLatin1String("\\\""), QLatin1String("\\\\\""));
        // escape a single " because the arguments will be parsed
        tmp.replace(QLatin1Char('\"'), QLatin1String("\\\""));
        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
            // The argument must not end with a \ since this would be interpreted
            // as escaping the quote -- rather put the \ behind the quote: e.g.
            // rather use "foo"\ than "foo\"
            QString endQuote(QLatin1Char('\"'));
            int i = tmp.length();
            while (i > 0 && tmp.at(i - 1) == QLatin1Char('\\')) {
                --i;
                endQuote += QLatin1Char('\\');
            }
            args += QLatin1String(" \"") + tmp.left(i) + endQuote;
        } else {
            args += QLatin1Char(' ') + tmp;
        }
    }
    return args;
}

QString QInstaller::createCommandline(const QString &program, const QStringList &arguments)
{
    return qt_create_commandline(program, arguments);
}

//copied from qsystemerror.cpp in Qt
QString QInstaller::windowsErrorString(int errorCode)
{
    QString ret;
    wchar_t *string = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &string,
        0,
        NULL);
    ret = QString::fromWCharArray(string);
    LocalFree((HLOCAL) string);

    if (ret.isEmpty() && errorCode == ERROR_MOD_NOT_FOUND)
        ret = QCoreApplication::translate("QInstaller", "The specified module could not be found.");

    ret.append(QLatin1String(" (0x"));
    ret.append(QString::number(uint(errorCode), 16).rightJustified(8, QLatin1Char('0')));
    ret.append(QLatin1String(")"));

    return ret;
}

#endif


