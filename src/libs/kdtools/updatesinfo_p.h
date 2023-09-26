/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef UPDATESINFO_P_H
#define UPDATESINFO_P_H

#include "updater.h"
#include "updatesinfodata_p.h"

#include <QHash>
#include <QSharedData>
#include <QVariant>

// They are not a part of the public API
// Classes and structures in this header file are for internal use only but still exported for auto tests.

namespace KDUpdater {

struct KDTOOLS_EXPORT UpdateInfo
{
    QHash<QString, QVariant> data;
};

class KDTOOLS_EXPORT UpdatesInfo
{
public:
    enum Error
    {
        NoError = 0,
        NotYetReadError,
        CouldNotReadUpdateInfoFileError,
        InvalidXmlError,
        InvalidContentError
    };

    UpdatesInfo(const bool postLoadComponentScript = false);
    ~UpdatesInfo();

    bool isValid() const;

    Error error() const;
    QString errorString() const;

    QString fileName() const;
    void setFileName(const QString &updateXmlFile);
    void parseFile();

    QString applicationName() const;
    QString applicationVersion() const;

    QString checkSha1CheckSum() const;

    int updateInfoCount() const;
    UpdateInfo updateInfo(int index) const;
    QList<UpdateInfo> updatesInfo() const;

private:
    QSharedDataPointer<UpdatesInfoData> d;
};

} // namespace KDUpdater

#endif // UPDATESINFO_P_H
