/****************************************************************************
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
****************************************************************************/
#include "packagemanagercoredata.h"

#include "errors.h"
#include "fileutils.h"
#include "qsettingswrapper.h"
#include "utils.h"

#include <QDesktopServices>
#include <QDir>
#include <QRegExp>

#ifdef Q_OS_WIN
# include <windows.h>
# include <shlobj.h>
#endif

namespace QInstaller
{

PackageManagerCoreData::PackageManagerCoreData(const QHash<QString, QString> &variables)
{
    m_variables = variables;
    setDynamicPredefinedVariables();

    // Set some common variables that may used e.g. as placeholder in some of the settings variables or
    // in a script or...
    m_variables.insert(scTargetConfigurationFile, QLatin1String("components.xml"));
    m_variables.insert(QLatin1String("InstallerDirPath"), QCoreApplication::applicationDirPath());
    m_variables.insert(QLatin1String("InstallerFilePath"), QCoreApplication::applicationFilePath());

#ifdef Q_OS_WIN
    m_variables.insert(QLatin1String("os"), QLatin1String("win"));
#elif defined(Q_OS_OSX)
    m_variables.insert(QLatin1String("os"), QLatin1String("mac"));
#elif defined(Q_OS_LINUX)
    m_variables.insert(QLatin1String("os"), QLatin1String("x11"));
#else
    // TODO: add more platforms as needed...
#endif

    m_settings = Settings::fromFileAndPrefix(QLatin1String(":/metadata/installer-config/config.xml"),
        QLatin1String(":/metadata/installer-config/"), Settings::RelaxedParseMode);

    // fill the variables defined in the settings
    m_variables.insert(QLatin1String("ProductName"), m_settings.applicationName());
    m_variables.insert(QLatin1String("ProductVersion"), m_settings.version());
    m_variables.insert(scTitle, m_settings.title());
    m_variables.insert(scPublisher, m_settings.publisher());
    m_variables.insert(QLatin1String("Url"), m_settings.url());
    m_variables.insert(scStartMenuDir, m_settings.startMenuDir());
    m_variables.insert(scTargetConfigurationFile, m_settings.configurationFileName());
    m_variables.insert(QLatin1String("LogoPixmap"), m_settings.logo());
    m_variables.insert(QLatin1String("WatermarkPixmap"), m_settings.watermark());
    m_variables.insert(QLatin1String("BannerPixmap"), m_settings.banner());

    const QString description = m_settings.runProgramDescription();
    if (!description.isEmpty())
        m_variables.insert(scRunProgramDescription, description);

    m_variables.insert(scTargetDir, replaceVariables(m_settings.targetDir()));
    m_variables.insert(scRemoveTargetDir, replaceVariables(m_settings.removeTargetDir()));
}

void PackageManagerCoreData::clear()
{
    m_variables.clear();
    m_settings = Settings();
}

/*!
    Set some common variables that may be used e.g. as placeholder in some of the settings
    variables or in a script or...
*/
void PackageManagerCoreData::setDynamicPredefinedVariables()
{
    m_variables.insert(QLatin1String("rootDir"), QDir::rootPath());
    m_variables.insert(QLatin1String("homeDir"), QDir::homePath());
    m_variables.insert(QLatin1String("RootDir"), QDir::rootPath());
    m_variables.insert(QLatin1String("HomeDir"), QDir::homePath());

    QString dir = QLatin1String("/opt");
#ifdef Q_OS_WIN
    TCHAR buffer[MAX_PATH + 1] = { 0 };
    SHGetFolderPath(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, buffer);
    dir = QString::fromWCharArray(buffer);
#elif defined (Q_OS_OSX)
    dir = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation).value(0);
#endif
    m_variables.insert(QLatin1String("ApplicationsDir"), dir);

    QString dirX86 = dir;
    QString dirX64 = dir;
#ifdef Q_OS_WIN
    QSettingsWrapper current(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion")
                          , QSettingsWrapper::NativeFormat);
    BOOL onWow64Or64bit = TRUE;
#ifndef Q_OS_WIN64
    IsWow64Process(GetCurrentProcess(), &onWow64Or64bit);
#endif
    QString programfilesX86;
    QString programfilesX64;
    if (onWow64Or64bit == TRUE) {
        programfilesX86 = current.value(QLatin1String("ProgramFilesDir (x86)"), QString()).toString();
        programfilesX64 = current.value(QLatin1String("ProgramW6432Dir"), QString()).toString();
    } else {
        programfilesX86 = current.value(QLatin1String("ProgramFilesDir"), QString()).toString();
        programfilesX64 = programfilesX86;
    }
    dirX86 = replaceWindowsEnvironmentVariables(programfilesX86);
    dirX64 = replaceWindowsEnvironmentVariables(programfilesX64);
#endif
    m_variables.insert(QLatin1String("ApplicationsDirX86"), dirX86);
    m_variables.insert(QLatin1String("ApplicationsDirX64"), dirX64);

#ifdef Q_OS_WIN
    QSettingsWrapper user(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\User Shell Folders"), QSettingsWrapper::NativeFormat);
    QSettingsWrapper system(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\Shell Folders"), QSettingsWrapper::NativeFormat);

    const QString programs = user.value(QLatin1String("Programs"), QString()).toString();
    const QString allPrograms = system.value(QLatin1String("Common Programs"), QString())
        .toString();

    QString desktop;
    if (m_variables.value(QLatin1String("AllUsers")) == scTrue) {
        desktop = system.value(QLatin1String("Desktop")).toString();
    } else {
        desktop = user.value(QLatin1String("Desktop")).toString();
    }
    m_variables.insert(QLatin1String("DesktopDir"), replaceWindowsEnvironmentVariables(desktop));
    m_variables.insert(QLatin1String("UserStartMenuProgramsPath"),
        replaceWindowsEnvironmentVariables(programs));
    m_variables.insert(QLatin1String("AllUsersStartMenuProgramsPath"),
        replaceWindowsEnvironmentVariables(allPrograms));
#endif
}

Settings &PackageManagerCoreData::settings() const
{
    return m_settings;
}

QStringList PackageManagerCoreData::keys() const
{
    return m_variables.keys();
}

bool PackageManagerCoreData::contains(const QString &key) const
{
    return m_variables.contains(key) || m_settings.containsValue(key);
}

bool PackageManagerCoreData::setValue(const QString &key, const QString &normalizedValue)
{
    if (m_variables.contains(key) && m_variables.value(key) == normalizedValue)
        return false;
    m_variables.insert(key, normalizedValue);
    return true;
}

QVariant PackageManagerCoreData::value(const QString &key, const QVariant &_default) const
{
    if (key == scTargetDir) {
        QString dir = m_variables.value(key);
        if (dir.isEmpty())
            dir = replaceVariables(m_settings.value(key, _default).toString());
#ifdef Q_OS_WIN
        return QInstaller::normalizePathName(dir);
#else
        if (dir.startsWith(QLatin1String("~/")))
            return QDir::home().absoluteFilePath(dir.mid(2));
        return dir;
#endif
    }

#ifdef Q_OS_WIN
    if (!m_variables.contains(key)) {
        static const QRegExp regex(QLatin1String("\\\\|/"));
        const QString filename = key.section(regex, 0, -2);
        const QString regKey = key.section(regex, -1);
        const QSettingsWrapper registry(filename, QSettingsWrapper::NativeFormat);
        if (!filename.isEmpty() && !regKey.isEmpty() && registry.contains(regKey))
            return registry.value(regKey).toString();
    }
#endif

    if (m_variables.contains(key))
        return m_variables.value(key);

    return m_settings.value(key, _default);
}

QString PackageManagerCoreData::replaceVariables(const QString &str) const
{
    static const QChar at = QLatin1Char('@');
    QString res;
    int pos = 0;
    while (true) {
        const int pos1 = str.indexOf(at, pos);
        if (pos1 == -1)
            break;
        const int pos2 = str.indexOf(at, pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        const QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += value(name).toString();
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QByteArray PackageManagerCoreData::replaceVariables(const QByteArray &ba) const
{
    static const QChar at = QLatin1Char('@');
    QByteArray res;
    int pos = 0;
    while (true) {
        const int pos1 = ba.indexOf(at, pos);
        if (pos1 == -1)
            break;
        const int pos2 = ba.indexOf(at, pos1 + 1);
        if (pos2 == -1)
            break;
        res += ba.mid(pos, pos1 - pos);
        const QString name = QString::fromLocal8Bit(ba.mid(pos1 + 1, pos2 - pos1 - 1));
        res += value(name).toString().toLocal8Bit();
        pos = pos2 + 1;
    }
    res += ba.mid(pos);
    return res;
}

}   // namespace QInstaller
