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

#include "kdupdaterupdatesinfo_p.h"
#include "utils.h"

#include <QFile>
#include <QLocale>
#include <QPair>
#include <QVector>
#include <QUrl>

using namespace KDUpdater;

UpdatesInfoData::UpdatesInfoData()
     : error(UpdatesInfo::NotYetReadError)
{
}

UpdatesInfoData::~UpdatesInfoData()
{
}

void UpdatesInfoData::setInvalidContentError(const QString &detail)
{
    error = UpdatesInfo::InvalidContentError;
    errorMessage = tr("Updates.xml contains invalid content: %1").arg(detail);
}

void UpdatesInfoData::parseFile(const QString &updateXmlFile)
{
    QFile file(updateXmlFile);
    if (!file.open(QFile::ReadOnly)) {
        error = UpdatesInfo::CouldNotReadUpdateInfoFileError;
        errorMessage = tr("Could not read \"%1\"").arg(updateXmlFile);
        return;
    }

    QDomDocument doc;
    QString parseErrorMessage;
    int parseErrorLine, parseErrorColumn;
    if (!doc.setContent(&file, &parseErrorMessage, &parseErrorLine, &parseErrorColumn)) {
        error = UpdatesInfo::InvalidXmlError;
        errorMessage = tr("Parse error in %1 at %2, %3: %4").arg(updateXmlFile,
            QString::number(parseErrorLine), QString::number(parseErrorColumn), parseErrorMessage);
        return;
    }

    QDomElement rootE = doc.documentElement();
    if (rootE.tagName() != QLatin1String("Updates")) {
        setInvalidContentError(tr("Root element %1 unexpected, should be \"Updates\".").arg(rootE.tagName()));
        return;
    }

    QDomNodeList childNodes = rootE.childNodes();
    for(int i = 0; i < childNodes.count(); i++) {
        const QDomElement childE = childNodes.at(i).toElement();
        if (childE.isNull())
            continue;

        if (childE.tagName() == QLatin1String("ApplicationName"))
            applicationName = childE.text();
        else if (childE.tagName() == QLatin1String("ApplicationVersion"))
            applicationVersion = childE.text();
        else if (childE.tagName() == QLatin1String("PackageUpdate")) {
            if (!parsePackageUpdateElement(childE))
                return; //error handled in subroutine
        }
    }

    if (applicationName.isEmpty()) {
        setInvalidContentError(tr("ApplicationName element is missing."));
        return;
    }

    if (applicationVersion.isEmpty()) {
        setInvalidContentError(tr("ApplicationVersion element is missing."));
        return;
    }

    errorMessage.clear();
    error = UpdatesInfo::NoError;
}

bool UpdatesInfoData::parsePackageUpdateElement(const QDomElement &updateE)
{
    if (updateE.isNull())
        return false;

    UpdateInfo info;
    QMap<QString, QString> localizedDescriptions;
    for (int i = 0; i < updateE.childNodes().count(); i++) {
        QDomElement childE = updateE.childNodes().at(i).toElement();
        if (childE.isNull())
            continue;

        if (childE.tagName() == QLatin1String("ReleaseNotes")) {
            info.data[childE.tagName()] = QUrl(childE.text());
        } else if (childE.tagName() == QLatin1String("Licenses")) {
            QHash<QString, QVariant> licenseHash;
            const QDomNodeList licenseNodes = childE.childNodes();
            for (int i = 0; i < licenseNodes.count(); ++i) {
                const QDomNode licenseNode = licenseNodes.at(i);
                if (licenseNode.nodeName() == QLatin1String("License")) {
                    QDomElement element = licenseNode.toElement();
                    licenseHash.insert(element.attributeNode(QLatin1String("name")).value(),
                        element.attributeNode(QLatin1String("file")).value());
                }
            }
            if (!licenseHash.isEmpty())
                info.data.insert(QLatin1String("Licenses"), licenseHash);
        } else if (childE.tagName() == QLatin1String("Version")) {
            info.data.insert(QLatin1String("inheritVersionFrom"),
                childE.attribute(QLatin1String("inheritVersionFrom")));
            info.data[childE.tagName()] = childE.text();
        } else if (childE.tagName() == QLatin1String("Description")) {
            if (!childE.hasAttribute(QLatin1String("xml:lang")))
                info.data[QLatin1String("Description")] = childE.text();
            QString languageAttribute = childE.attribute(QLatin1String("xml:lang"), QLatin1String("en"));
            localizedDescriptions.insert(languageAttribute.toLower(), childE.text());
        } else if (childE.tagName() == QLatin1String("UpdateFile")) {
            info.data[QLatin1String("CompressedSize")] = childE.attribute(QLatin1String("CompressedSize"));
            info.data[QLatin1String("UncompressedSize")] = childE.attribute(QLatin1String("UncompressedSize"));
        } else {
            info.data[childE.tagName()] = childE.text();
        }
    }

    QStringList candidates;
    foreach (const QString &lang, QLocale().uiLanguages())
        candidates << QInstaller::localeCandidates(lang.toLower());
    foreach (const QString &candidate, candidates) {
        if (localizedDescriptions.contains(candidate)) {
            info.data[QLatin1String("Description")] = localizedDescriptions.value(candidate);
            break;
        }
    }

    if (!info.data.contains(QLatin1String("Name"))) {
        setInvalidContentError(tr("PackageUpdate element without Name"));
        return false;
    }
    if (!info.data.contains(QLatin1String("Version"))) {
        setInvalidContentError(tr("PackageUpdate element without Version"));
        return false;
    }
    if (!info.data.contains(QLatin1String("ReleaseDate"))) {
        setInvalidContentError(tr("PackageUpdate element without ReleaseDate"));
        return false;
    }

    updateInfoList.append(info);
    return true;
}


//
// UpdatesInfo
//
UpdatesInfo::UpdatesInfo()
    : d(new UpdatesInfoData)
{
}

UpdatesInfo::~UpdatesInfo()
{
}

bool UpdatesInfo::isValid() const
{
    return d->error == NoError;
}

QString UpdatesInfo::errorString() const
{
    return d->errorMessage;
}

void UpdatesInfo::setFileName(const QString &updateXmlFile)
{
    if (d->updateXmlFile == updateXmlFile)
        return;

    d->applicationName.clear();
    d->applicationVersion.clear();
    d->updateInfoList.clear();

    d->updateXmlFile = updateXmlFile;
    d->parseFile(d->updateXmlFile);
}

QString UpdatesInfo::fileName() const
{
    return d->updateXmlFile;
}

QString UpdatesInfo::applicationName() const
{
    return d->applicationName;
}

QString UpdatesInfo::applicationVersion() const
{
    return d->applicationVersion;
}

int UpdatesInfo::updateInfoCount() const
{
    return d->updateInfoList.count();
}

UpdateInfo UpdatesInfo::updateInfo(int index) const
{
    if (index < 0 || index >= d->updateInfoList.count())
        return UpdateInfo();
    return d->updateInfoList.at(index);
}

QList<UpdateInfo> UpdatesInfo::updatesInfo() const
{
    return d->updateInfoList;
}
