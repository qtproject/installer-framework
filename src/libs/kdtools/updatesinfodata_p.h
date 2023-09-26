/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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

#ifndef UPDATESINFODATA_P_H
#define UPDATESINFODATA_P_H

#include <QCoreApplication>
#include <QSharedData>

QT_FORWARD_DECLARE_CLASS(QXmlStreamReader)

namespace KDUpdater {

struct UpdateInfo;

struct UpdatesInfoData : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::UpdatesInfoData)

public:
    UpdatesInfoData(const bool postLoadComponentScript);
    ~UpdatesInfoData();

    int error;
    QString errorMessage;
    QString updateXmlFile;
    QString applicationName;
    QString applicationVersion;
    QString checkSha1CheckSum;
    QList<UpdateInfo> updateInfoList;
    bool m_postLoadComponentScript;

    void parseFile(const QString &updateXmlFile);
    bool parsePackageUpdateElement(QXmlStreamReader &reader, const QString &checkSha1CheckSum);

    void setInvalidContentError(const QString &detail);

private:
    void processLocalizedTag(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
    void parseOperations(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
    void parseLicenses(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
};

} // namespace KDUpdater

#endif // UPDATESINFODATA_P_H
