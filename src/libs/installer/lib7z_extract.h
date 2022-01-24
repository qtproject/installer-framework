/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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
#ifndef LIB7Z_EXTRACT_H
#define LIB7Z_EXTRACT_H

#include "installer_global.h"

#include <Common/MyCom.h>
#include <7zip/Archive/IArchive.h>

#include <QString>

class CArc;

QT_BEGIN_NAMESPACE
class QFileDevice;
QT_END_NAMESPACE

namespace Lib7z
{
    class INSTALLER_EXPORT ExtractCallback : public IArchiveExtractCallback, public CMyUnknownImp
    {
        Q_DISABLE_COPY(ExtractCallback)

    public:
        ExtractCallback() = default;
        virtual ~ExtractCallback() = default;

        void setArchive(CArc *carc) { arc = carc; }
        void setTarget(const QString &dir) { targetDir = dir; }

        MY_UNKNOWN_IMP
        INTERFACE_IArchiveExtractCallback(;)

    protected:
        virtual bool prepareForFile(const QString & /*filename*/) { return true; }
        virtual void setCurrentFile(const QString &filename) { Q_UNUSED(filename) }
        virtual HRESULT setCompleted(quint64 /*completed*/, quint64 /*total*/) { return S_OK; }

    private:
        CArc *arc = 0;

        QString targetDir;
        quint64 total = 0;
        quint64 completed = 0;
        quint32 currentIndex = 0;
    };

    void INSTALLER_EXPORT extractArchive(QFileDevice *archive, const QString &targetDirectory,
        ExtractCallback *callback = 0);

} // namespace Lib7z

#endif // LIB7Z_EXTRACT_H
