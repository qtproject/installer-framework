/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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

} // namespace CommandLineOptions

#endif // CONSTANTS_H
