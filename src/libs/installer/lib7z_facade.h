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
#ifndef LIB7Z_FACADE_H
#define LIB7Z_FACADE_H

#include "installer_global.h"
#include "errors.h"

#include <Common/MyWindows.h>
#include <7zip/UI/Console/PercentPrinter.h>

QT_BEGIN_NAMESPACE
class QFileDevice;
QT_END_NAMESPACE

namespace Lib7z
{
    void INSTALLER_EXPORT initSevenZ();
    bool INSTALLER_EXPORT isSupportedArchive(QFileDevice *archive);
    bool INSTALLER_EXPORT isSupportedArchive(const QString &archive);

    class INSTALLER_EXPORT SevenZipException : public QInstaller::Error
    {
    public:
        explicit SevenZipException(const QString &msg)
            : QInstaller::Error(msg)
        {}

        explicit SevenZipException(const char *msg)
            : QInstaller::Error(QString::fromLocal8Bit(msg))
        {}
    };

    class INSTALLER_EXPORT PercentPrinter : public CPercentPrinter
    {
    public:
        PercentPrinter() : CPercentPrinter(1 << 16) {
            OutStream = &g_StdOut;
        }

        void PrintRatio() { CPercentPrinter::PrintRatio(); }
        void ClosePrint() { CPercentPrinter::ClosePrint(); }
        void RePrintRatio() { CPercentPrinter::RePrintRatio(); }
        void PrintNewLine() { CPercentPrinter::PrintNewLine(); }
        void PrintString(const char *s) { CPercentPrinter::PrintString(s); }
        void PrintString(const wchar_t *s) { CPercentPrinter::PrintString(s); }
    };

} // namespace Lib7z

#endif // LIB7Z_FACADE_H
