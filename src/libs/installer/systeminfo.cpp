/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "systeminfo.h"
#include <QSysInfo>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::SystemInfo

    \brief Provides information about the operating system.
*/

/*!
    Creates a system info object with the parent \a parent.
*/
SystemInfo::SystemInfo(QObject *parent) : QObject(parent)
{
}


/*!
    \property SystemInfo::currentCpuArchitecture

    The architecture of the CPU that the application is running on, in text format.

    Possible values include:
    \list
        \li "i386"
        \li "x86_64"
        \li "arm64"
    \endlist

    \note This function depends on what the OS will report and may not detect the actual CPU
    architecture if the OS hides that information or is unable to provide it. For example, a 32-bit
    OS running on a 64-bit CPU is usually unable to determine whether the CPU is actually capable
    of running 64-bit programs.

    \sa QSysInfo::currentCpuArchitecture() buildCpuArchitecture()
*/
QString SystemInfo::currentCpuArchitecture() const
{
    return QSysInfo::currentCpuArchitecture();
}

/*!
    \property SystemInfo::buildCpuArchitecture

    The architecture of the CPU that the application was compiled for, in text format.

    Possible values include:
    \list
        \li "i386"
        \li "x86_64"
        \li "arm64"
    \endlist

    \note  Note that this may not match the actual CPU that the application is running on if
    there's an emulation layer or if the CPU supports multiple architectures (like x86-64
    processors supporting i386 applications). To detect that, use \c installer.currentCpuArchitecture()

    \sa QSysInfo::buildCpuArchitecture() currentCpuArchitecture()
*/
QString SystemInfo::buildCpuArchitecture() const
{
    return QSysInfo::buildCpuArchitecture();
}

/*!
    \property SystemInfo::kernelType

    The type of the operating system kernel the installer was compiled for. It is also the
    kernel the installer is running on, unless the host operating system is running a form of
    compatibility or virtualization layer.

    For Windows, Linux, and macOS this will return:
    \list
        \li "winnt"
        \li "linux"
        \li "darwin"
    \endlist

    On Unix systems, it returns the same as the output of \c {uname -s} (lowercased).

    \sa QSysInfo::kernelType()
*/
QString SystemInfo::kernelType() const
{
    return QSysInfo::kernelType();
}

/*!
    \property SystemInfo::kernelVersion

    The release version of the operating system kernel. On Windows, it returns the version of the
    NT or CE kernel. On Unix systems, including macOS, it returns the same as the \c {uname -r}
    command would return.

    Example values are:

    \list
        \li "6.1.7601" for Windows 7 with Service Pack 1
        \li "3.16.6-2-desktop" for openSUSE 13.2 kernel 3.16.6-2
        \li "12.5.0" last release of OS X "Mountain Lion"
    \endlist

    \sa QSysInfo::kernelVersion()
*/
QString SystemInfo::kernelVersion() const
{
    return QSysInfo::kernelVersion();
}

/*!
    \property SystemInfo::productType

    The product name of the operating system this application is running in.

    Example values are:

    \list
        \li "windows"
        \li "opensuse" (for the Linux openSUSE distribution)
        \li "macos"
    \endlist

    \sa QSysInfo::productType()
*/
QString SystemInfo::productType() const
{
    return QSysInfo::productType();
}

/*!
    \property SystemInfo::productVersion

    The product version of the operating system in string form. If the version could not be
    determined, this function returns "unknown".

    Example values are:

    \list
        \li "7" for Windows 7
        \li "13.2" for openSUSE 13.2
        \li "10.8" for OS X Mountain Lion
    \endlist

    \sa QSysInfo::productVersion()
*/
QString SystemInfo::productVersion() const
{
    return QSysInfo::productVersion();
}

/*!
    \property SystemInfo::prettyProductName

    A prettier form of SystemInfo::productType and SystemInfo::productVersion, containing other
    tokens like the operating system type, codenames and other information. The result of this
    function is suitable for displaying to the user.

    Example values are:

    \list
        \li "Windows 7"
        \li "openSUSE 13.2 (Harlequin) (x86_64)"
        \li "OS X Mountain Lion (10.8)"
    \endlist


    \sa QSysInfo::prettyProductName()
*/
QString SystemInfo::prettyProductName() const
{
    return QSysInfo::prettyProductName();
}

} // namespace QInstaller
