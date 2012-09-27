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

#include <QCoreApplication>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QFile>
#include <QSharedData>
#include <QLocale>

using namespace KDUpdater;

//
// UpdatesInfo::UpdatesInfoData
//
struct UpdatesInfo::UpdatesInfoData : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::UpdatesInfoData)

public:
    UpdatesInfoData() : error(UpdatesInfo::NotYetReadError), compatLevel(-1) { }

    QString errorMessage;
    UpdatesInfo::Error error;
    QString updateXmlFile;
    QString applicationName;
    QString applicationVersion;
    int compatLevel;
    QList<UpdateInfo> updateInfoList;

    void parseFile(const QString &updateXmlFile);
    bool parsePackageUpdateElement(const QDomElement &updateE);
    bool parseCompatUpdateElement(const QDomElement &updateE);

    void setInvalidContentError(const QString &detail);
};

void UpdatesInfo::UpdatesInfoData::setInvalidContentError(const QString &detail)
{
    error = UpdatesInfo::InvalidContentError;
    errorMessage = tr("Updates.xml contains invalid content: %1").arg(detail);
}

void UpdatesInfo::UpdatesInfoData::parseFile(const QString &updateXmlFile)
{
    QFile file(updateXmlFile);
    if (!file.open(QFile::ReadOnly)) {
        error = UpdatesInfo::CouldNotReadUpdateInfoFileError;
        errorMessage = tr("Could not read \"%1\"").arg(updateXmlFile);
        return;
    }

    QDomDocument doc;
    QString parseErrorMessage;
    int parseErrorLine;
    int parseErrorColumn;
    if (!doc.setContent(&file, &parseErrorMessage, &parseErrorLine, &parseErrorColumn)) {
        error = UpdatesInfo::InvalidXmlError;
        errorMessage = tr("Parse error in %1 at %2, %3: %4")
                      .arg(updateXmlFile,
                           QString::number(parseErrorLine),
                           QString::number(parseErrorColumn),
                           parseErrorMessage);
        return;
    }

    QDomElement rootE = doc.documentElement();
    if (rootE.tagName() != QLatin1String("Updates")) {
        setInvalidContentError(tr("Root element %1 unexpected, should be \"Updates\".").arg(rootE.tagName()));
        return;
    }

    QDomNodeList childNodes = rootE.childNodes();
    for(int i = 0; i < childNodes.count(); i++) {
        QDomNode childNode = childNodes.at(i);
        QDomElement childE = childNode.toElement();
        if (childE.isNull())
            continue;

        if (childE.tagName() == QLatin1String("ApplicationName"))
            applicationName = childE.text();
        else if (childE.tagName() == QLatin1String("ApplicationVersion"))
            applicationVersion = childE.text();
        else if (childE.tagName() == QLatin1String("RequiredCompatLevel"))
            compatLevel = childE.text().toInt();
        else if (childE.tagName() == QLatin1String("PackageUpdate")) {
            const bool res = parsePackageUpdateElement(childE);
            if (!res) {
                //error handled in subroutine
                return;
            }
        } else if (childE.tagName() == QLatin1String("CompatUpdate")) {
            const bool res = parseCompatUpdateElement(childE);
            if (!res) {
                //error handled in subroutine
                return;
            }
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
    
    error = UpdatesInfo::NoError;
    errorMessage.clear();
}

bool UpdatesInfo::UpdatesInfoData::parsePackageUpdateElement(const QDomElement &updateE)
{
    if (updateE.isNull())
        return false;

    UpdateInfo info;
    info.type = PackageUpdate;

    QDomNodeList childNodes = updateE.childNodes();
    for (int i = 0; i < childNodes.count(); i++) {
        QDomNode childNode = childNodes.at(i);
        QDomElement childE = childNode.toElement();
        if (childE.isNull())
            continue;

        if (childE.tagName() == QLatin1String("ReleaseNotes")) {
            info.data[childE.tagName()] = QUrl(childE.text());
        } else if (childE.tagName() == QLatin1String("UpdateFile")) {
            UpdateFileInfo ufInfo;
            ufInfo.compressedSize = childE.attribute(QLatin1String("CompressedSize")).toLongLong();
            ufInfo.uncompressedSize = childE.attribute(QLatin1String("UncompressedSize")).toLongLong();
            ufInfo.sha1sum = QByteArray::fromHex(childE.attribute(QLatin1String("sha1sum")).toAscii());
            ufInfo.fileName = childE.text();
            info.updateFiles.append(ufInfo);
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
            info.data.insert(QLatin1String("inheritVersionFrom"), childE.attribute(QLatin1String("inheritVersionFrom")));
            info.data[childE.tagName()] = childE.text();
        } else if (childE.tagName() == QLatin1String("Description")) {

            QString languageAttribute = childE.attribute(QLatin1String("xml:lang"));

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
    if (info.updateFiles.isEmpty()) {
        setInvalidContentError(tr("PackageUpdate element without UpdateFile"));
        return false;
    }

    updateInfoList.append(info);
    return true;
}

bool UpdatesInfo::UpdatesInfoData::parseCompatUpdateElement(const QDomElement &updateE)
{
    if (updateE.isNull())
        return false;

    UpdateInfo info;
    info.type = CompatUpdate;

    QDomNodeList childNodes = updateE.childNodes();
    for (int i = 0; i < childNodes.count(); i++) {
        QDomNode childNode = childNodes.at(i);
        QDomElement childE = childNode.toElement();
        if (childE.isNull())
            continue;

        if (childE.tagName() == QLatin1String("ReleaseNotes")) {
            info.data[childE.tagName()] = QUrl(childE.text());
        } else if (childE.tagName() == QLatin1String("UpdateFile")) {
            UpdateFileInfo ufInfo;
            ufInfo.fileName = childE.text();
            info.updateFiles.append(ufInfo);
        } else {
            info.data[childE.tagName()] = childE.text();
        }
    }

    if (!info.data.contains(QLatin1String("CompatLevel"))) {
        setInvalidContentError(tr("CompatUpdate element without CompatLevel"));
        return false;
    }
    
    if (!info.data.contains(QLatin1String("ReleaseDate"))) {
        setInvalidContentError(tr("CompatUpdate element without ReleaseDate"));
        return false;
    }
    
    if (info.updateFiles.isEmpty()) {
        setInvalidContentError(tr("CompatUpdate element without UpdateFile"));
        return false;
    }

    updateInfoList.append(info);
    return true;
}


//
// UpdatesInfo
//
UpdatesInfo::UpdatesInfo()
    : d(new UpdatesInfo::UpdatesInfoData)
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

int UpdatesInfo::compatLevel() const
{
    return d->compatLevel;
}

int UpdatesInfo::updateInfoCount(int type) const
{
    if (type == AllUpdate)
        return d->updateInfoList.count();

    int count = 0;
    for (int i = 0; i < d->updateInfoList.count(); ++i) {
        if (d->updateInfoList.at(i).type == type)
            ++count;
    }
    return count;
}

UpdateInfo UpdatesInfo::updateInfo(int index) const
{
    if (index < 0 || index >= d->updateInfoList.count())
        return UpdateInfo();

    return d->updateInfoList.at(index);
}

QList<UpdateInfo> UpdatesInfo::updatesInfo(int type, int compatLevel) const
{
    QList<UpdateInfo> list;
    if (compatLevel == -1) {
        if (type == AllUpdate)
            return d->updateInfoList;
        for (int i = 0; i < d->updateInfoList.count(); ++i) {
            if (d->updateInfoList.at(i).type == type)
                list.append(d->updateInfoList.at(i));
        }
    } else {
        for (int i = 0; i < d->updateInfoList.count(); ++i) {
            UpdateInfo updateInfo = d->updateInfoList.at(i);
            if (updateInfo.type == type) {
                if (updateInfo.type == CompatUpdate) {
                    if (updateInfo.data.value(QLatin1String("CompatLevel")) == compatLevel)
                        list.append(updateInfo);
                } else {
                    if (updateInfo.data.value(QLatin1String("RequiredCompatLevel")) == compatLevel)
                        list.append(updateInfo);
                }
            }
        }
    }
    return list;
}
