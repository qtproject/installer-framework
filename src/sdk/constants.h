/**************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef CONSTANTS_H
#define CONSTANTS_H

namespace CommandLineOptions {

const char HelpShort[] = "h";
const char HelpLong[] = "help";
const char Version[] = "version";
const char FrameworkVersion[] = "framework-version";
const char VerboseShort[] = "v";
const char VerboseLong[] = "verbose";
const char Proxy[] = "proxy";
const char NoProxy[] = "no-proxy";
const char Script[] = "script";
const char CheckUpdates[] = "checkupdates";
const char Updater[] = "updater";
const char ManagePackages[] = "manage-packages";
const char NoForceInstallation[] = "no-force-installations";
const char ShowVirtualComponents[] = "show-virtual-components";
const char LoggingRules[] = "logging-rules";
const char CreateLocalRepository[] = "create-local-repository";
const char AddRepository[] = "addRepository";
const char AddTmpRepository[] = "addTempRepository";
const char SetTmpRepository[] = "setTempRepository";
const char StartServer[] = "startserver";
const char StartClient[] = "startclient";
const char InstallCompressedRepository[] = "installCompressedRepository";
const char SilentUpdate[] = "silentUpdate";
const char Platform[] = "platform";
const char SquishPort[] = "squish-port";

} // namespace CommandLineOptions

#endif // CONSTANTS_H
