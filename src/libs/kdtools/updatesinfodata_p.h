/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef UPDATESINFODATA_P_H
#define UPDATESINFODATA_P_H

#include <QCoreApplication>
#include <QSharedData>

QT_FORWARD_DECLARE_CLASS(QDomElement)

namespace KDUpdater {

struct UpdateInfo;

struct UpdatesInfoData : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::UpdatesInfoData)

public:
    UpdatesInfoData();
    ~UpdatesInfoData();

    int error;
    QString errorMessage;
    QString updateXmlFile;
    QString applicationName;
    QString applicationVersion;
    QList<UpdateInfo> updateInfoList;

    void parseFile(const QString &updateXmlFile);
    bool parsePackageUpdateElement(const QDomElement &updateE);

    void setInvalidContentError(const QString &detail);

private:
    void processLocalizedTag(const QDomElement &childE, QHash<QString, QVariant> &info) const;
};

} // namespace KDUpdater

#endif // UPDATESINFODATA_P_H
