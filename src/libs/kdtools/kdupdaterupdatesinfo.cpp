/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "kdupdaterupdatesinfo_p.h"

#include <QFile>
#include <QLocale>
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
            QString languageAttribute = childE.attribute(QLatin1String("xml:lang")).toLower();
            if (!info.data.contains(QLatin1String("Description")) && (languageAttribute.isEmpty()))
                info.data[childE.tagName()] = childE.text();

            // overwrite default if we have a language specific description
            if (languageAttribute == QLocale().name().toLower())
                info.data[childE.tagName()] = childE.text();
        } else {
            info.data[childE.tagName()] = childE.text();
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
