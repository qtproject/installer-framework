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
#include "utils.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QVector>

#if defined(Q_OS_WIN32) || defined(Q_OS_WINCE)
#   include "qt_windows.h"
#endif

#include <fstream>
#include <iostream>
#include <sstream>

static bool verb = false;

void QInstaller::setVerbose(bool v)
{
    verb = v;
}

bool QInstaller::isVerbose()
{
    return verb;
}

#ifdef Q_WS_WIN
void qWinMsgHandler(QtMsgType t, const char *str);

class debugstream : public std::ostream
{
    class buf : public std::stringbuf
    {
    public:
        buf() {}

        int sync()
        {
            std::string s = str();
            if (s[s.length() - 1] == '\n' )
                s[s.length() - 1] = '\0'; // remove \n
            qWinMsgHandler(QtDebugMsg, s.c_str());
            std::cout << s << std::endl;
            str(std::string());
            return 0;
        }
    };
public:
    debugstream() : std::ostream(&b) {}
private:
    buf b;
};
#endif

std::ostream &QInstaller::stdverbose()
{
    static std::fstream null;
#ifdef Q_WS_WIN
    static debugstream stream;
#else
    static std::ostream& stream = std::cout;
#endif
    if (verb)
        return stream;
    return null;
}

std::ostream &QInstaller::operator<<(std::ostream &os, const QString &string)
{
    return os << qPrintable(string);
}

//TODO from kdupdaterfiledownloader.cpp, use that one once merged
QByteArray QInstaller::calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo)
{
    Q_ASSERT(device);
    QCryptographicHash hash(algo);
    QByteArray buffer;
    buffer.resize(512 * 1024);
    while (true) {
        const qint64 numRead = device->read(buffer.data(), buffer.size());
        if (numRead <= 0)
            return hash.result();
        hash.addData(buffer.constData(), numRead);
    }
    return QByteArray(); // never reached
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

QInstaller::VerboseWriter::VerboseWriter(QObject *parent) : QObject(parent)
{
    preFileBuffer.open(QIODevice::ReadWrite);
    stream.setDevice(&preFileBuffer);
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
    if (output.open(QIODevice::ReadWrite | QIODevice::Append)) {
        QString logInfo;
        logInfo += QLatin1String("*************************************");
        logInfo += QLatin1String("Invoked:") + QDateTime::currentDateTime().toString();
        output.write(logInfo.toLocal8Bit());
        output.write(preFileBuffer.data());
        output.close();
    }
    stream.setDevice(0);
}

void QInstaller::VerboseWriter::setOutputStream(const QString &fileName)
{
    logFileName = fileName;
}


Q_GLOBAL_STATIC(QInstaller::VerboseWriter, verboseWriter)

QInstaller::VerboseWriter *QInstaller::VerboseWriter::instance()
{
    return verboseWriter();
}

QInstaller::VerboseWriter &QInstaller::verbose()
{
    return *verboseWriter();
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

        // add the prgram as the first arg ... it works better
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
#endif
