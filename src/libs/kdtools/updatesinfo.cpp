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

#include "updatesinfo_p.h"
#include "utils.h"
#include "constants.h"

#include <QFile>
#include <QLocale>
#include <QPair>
#include <QVector>
#include <QUrl>
#include <QXmlStreamReader>

using namespace KDUpdater;

UpdatesInfoData::UpdatesInfoData(const bool postLoadComponentScript)
     : error(UpdatesInfo::NotYetReadError)
    , m_postLoadComponentScript(postLoadComponentScript)
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
        errorMessage = tr("Cannot read \"%1\"").arg(updateXmlFile);
        return;
    }

    QXmlStreamReader reader(&file);
    if (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Updates")) {
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("ApplicationName")) {
                    applicationName = reader.readElementText();
                } else if (reader.name() == QLatin1String("ApplicationVersion")) {
                    applicationVersion = reader.readElementText();
                } else if (reader.name() == QLatin1String("Checksum")) {
                    checkSha1CheckSum = (reader.readElementText());
                } else if (reader.name() == QLatin1String("PackageUpdate")) {
                    if (!parsePackageUpdateElement(reader, checkSha1CheckSum))
                        return; //error handled in subroutine
                } else {
                    reader.skipCurrentElement();
                }
            }
        } else {
            setInvalidContentError(tr("Root element %1 unexpected, should be \"Updates\".").arg(reader.name()));
            return;
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

bool UpdatesInfoData::parsePackageUpdateElement(QXmlStreamReader &reader, const QString &checkSha1CheckSum)
{
    UpdateInfo info;
    QHash<QString, QVariant> scriptHash;
    while (reader.readNext()) {
        const QString elementName = reader.name().toString();
        if ((reader.name() == QLatin1String("PackageUpdate"))
                && (reader.tokenType() == QXmlStreamReader::EndElement)) {
            break;
        }
        if (elementName.isEmpty() || reader.tokenType() == QXmlStreamReader::EndElement)
            continue;
        if (elementName == QLatin1String("Licenses")) {
            parseLicenses(reader, info.data);
        } else if (elementName == QLatin1String("TreeName")) {
            const QXmlStreamAttributes attr = reader.attributes();
            const bool moveChildren = attr.value(QLatin1String("moveChildren")).toString().toLower() == QInstaller::scTrue ? true : false;
            const QPair<QString, bool> treeNamePair(reader.readElementText(), moveChildren);
            info.data.insert(QLatin1String("TreeName"), QVariant::fromValue(treeNamePair));
        } else if (elementName == QLatin1String("Version")) {
            const QXmlStreamAttributes attr = reader.attributes();
            info.data.insert(QLatin1String("inheritVersionFrom"),
                attr.value(QLatin1String("inheritVersionFrom")).toString());
            info.data[elementName] = reader.readElementText();
        } else if (elementName == QLatin1String("DisplayName")
                    || elementName == QLatin1String("Description")) {
            processLocalizedTag(reader, info.data);
        } else if (elementName == QLatin1String("UpdateFile")) {
            info.data[QLatin1String("CompressedSize")] = reader.attributes().value(QLatin1String("CompressedSize")).toString();
            info.data[QLatin1String("UncompressedSize")] = reader.attributes().value(QLatin1String("UncompressedSize")).toString();
        } else if (elementName == QLatin1String("Operations")) {
            parseOperations(reader, info.data);
        } else if (elementName == QLatin1String("Script")) {
            const QXmlStreamAttributes attr = reader.attributes();
            bool postLoad = false;
            // postLoad can be set either to individual components or to whole repositories.
            // If individual components has the postLoad attribute, it overwrites the repository value.
            if (attr.hasAttribute(QLatin1String("postLoad")))
                postLoad = attr.value(QLatin1String("postLoad")).toString().toLower() == QInstaller::scTrue ? true : false;
            else if (m_postLoadComponentScript)
                postLoad = true;

            if (postLoad)
                scriptHash.insert(QLatin1String("postLoadScript"), reader.readElementText());
            else
                scriptHash.insert(QLatin1String("installScript"), reader.readElementText());
        } else {
            info.data[elementName] = reader.readElementText();
        }
    }
    if (!scriptHash.isEmpty())
        info.data.insert(QLatin1String("Script"), scriptHash);

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
    info.data[QLatin1String("CheckSha1CheckSum")] = checkSha1CheckSum;
    updateInfoList.append(info);
    return true;
}

void UpdatesInfoData::processLocalizedTag(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const
{
    const QString languageAttribute =  reader.attributes().value(QLatin1String("xml:lang")).toString().toLower();
    const QString elementName = reader.name().toString();
    if (!info.contains(elementName) && (languageAttribute.isEmpty()))
        info[elementName] = reader.readElementText();

    if (languageAttribute.isEmpty())
        return;
    // overwrite default if we have a language specific description
    if (QLocale().name().startsWith(languageAttribute, Qt::CaseInsensitive))
        info[elementName] = reader.readElementText();
}

void UpdatesInfoData::parseOperations(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const
{
    QList<QPair<QString, QVariant>> operationsList;
    while (reader.readNext()) {
        const QString subElementName = reader.name().toString();
        // End of parsing operations
        if ((subElementName == QLatin1String("Operations"))
                && (reader.tokenType() == QXmlStreamReader::EndElement)) {
            break;
        }
        if (subElementName != QLatin1String("Operation") || reader.tokenType() == QXmlStreamReader::EndElement)
            continue;
        QStringList attributes;
        const QXmlStreamAttributes attr = reader.attributes();
        while (reader.readNext()) {
            const QString subElementName2 = reader.name().toString();
            // End of parsing single operation
            if ((subElementName2 == QLatin1String("Operation"))
                    && (reader.tokenType() == QXmlStreamReader::EndElement)) {
                break;
            }
            if (subElementName2 != QLatin1String("Argument") || reader.tokenType() == QXmlStreamReader::EndElement)
                continue;
            attributes.append(reader.readElementText());
        }
        QPair<QString, QVariant> pair;
        pair.first = attr.value(QLatin1String("name")).toString();
        pair.second = attributes;
        operationsList.append(pair);
    }
    info.insert(QLatin1String("Operations"), QVariant::fromValue(operationsList));
}

void UpdatesInfoData::parseLicenses(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const
{
    QHash<QString, QVariant> licenseHash;
    while (reader.readNext()) {
        const QString subElementName = reader.name().toString();
        // End of parsing Licenses
        if ((subElementName == QLatin1String("Licenses"))
                && (reader.tokenType() == QXmlStreamReader::EndElement)) {
            break;
        }
        if (subElementName != QLatin1String("License") || reader.tokenType() == QXmlStreamReader::EndElement)
            continue;
        const QXmlStreamAttributes attr = reader.attributes();
        QVariantMap attributes;
        attributes.insert(QLatin1String("file"), attr.value(QLatin1String("file")).toString());
        if (!attr.value(QLatin1String("priority")).isNull())
            attributes.insert(QLatin1String("priority"), attr.value(QLatin1String("priority")).toString());
        else
            attributes.insert(QLatin1String("priority"), QLatin1String("0"));
        licenseHash.insert(attr.value(QLatin1String("name")).toString(), attributes);
    }
    if (!licenseHash.isEmpty())
        info.insert(QLatin1String("Licenses"), licenseHash);
}
//
// UpdatesInfo
//
UpdatesInfo::UpdatesInfo(const bool postLoadComponentScript)
    : d(new UpdatesInfoData(postLoadComponentScript))
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
}

void UpdatesInfo::parseFile()
{
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

QString UpdatesInfo::checkSha1CheckSum() const
{
    return d->checkSha1CheckSum;
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
