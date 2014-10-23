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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "commandlineparser.h"

#include "constants.h"

namespace CommandLineOptions {
const char KeyValue[] = "Key=Value";
} // namespace CommandLineOptions

CommandLineParser::CommandLineParser()
{
    m_parser.addHelpOption();

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Version),
        QLatin1String("Displays version information.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::FrameworkVersion),
        QLatin1String("Displays the version of the Qt Installer Framework.")));

    m_parser.addOption(QCommandLineOption(QStringList()
        << QLatin1String(CommandLineOptions::VerboseShort)
        << QLatin1String(CommandLineOptions::VerboseLong),
        QLatin1String("Verbose mode. Prints out more information.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Proxy),
        QLatin1String("Use system proxy on Windows and OS X. This option has no effect on Linux.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Script),
        QLatin1String("Execute the script given as argument."), QLatin1String("file")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::CheckUpdates),
        QLatin1String("Check for updates and return an XML description.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::Updater),
        QLatin1String("Start application in updater mode.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::ManagePackages),
        QLatin1String("Start application in package manager mode.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::NoForceInstallation),
        QLatin1String("Allow deselection of components that are marked as forced.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::ShowVirtualComponents),
        QLatin1String("Show virtual components in installer and package manager.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::CreateOfflineRepository),
        QLatin1String("Create a local repository inside the installation directory. This option "
        "has no effect on online installers.")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::AddRepository),
        QLatin1String("Add a local or remote repository to the list of user defined repositories."),
        QLatin1String("URI,...")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::AddTmpRepository),
        QLatin1String("Add a local or remote repository to the list of temporary available "
        "repositories."), QLatin1String("URI,...")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::SetTmpRepository),
        QLatin1String("Set a local or remote repository as temporary repository, it is the only "
        "one used during fetch.\nNote: URI must be prefixed with the protocol, i.e. file:///, "
        "https://, http:// or ftp://."), QLatin1String("URI,...")));

    m_parser.addOption(QCommandLineOption(QLatin1String(CommandLineOptions::StartServer),
        QLatin1String("Starts the application as headless process waiting for commands to execute."),
        QLatin1String("port,key")));

    m_parser.addPositionalArgument(QLatin1String(CommandLineOptions::KeyValue),
        QLatin1String("Key Value pair to be set."));
}
