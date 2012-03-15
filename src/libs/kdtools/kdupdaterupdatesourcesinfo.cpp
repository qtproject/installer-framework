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

#include "kdupdaterupdatesourcesinfo.h"
#include "kdupdaterapplication.h"

#include <QDomElement>
#include <QDomDocument>
#include <QDomText>
#include <QDomCDATASection>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>


/*!
   \ingroup kdupdater
   \class KDUpdater::UpdateSourcesInfo kdupdaterupdatesourcesinfo.h KDUpdaterUpdateSourcesInfo
   \brief Provides access to information about the update sources set for the application.

   An update source is a repository that contains updates applicable for the application.
   Applications can download updates from the update source and install them locally.

   Each application can have one or more update sources from which it can download updates.
   Information about update source is stored in a file called UpdateSources.xml. This class helps
   access and modify the UpdateSources.xml file.

   The complete file name of the UpdateSources.xml file can be specified via the \ref setFileName()
   method. The class then parses the XML file and makes available information contained in
   that XML file through an easy to use API. You can

   \li Get update sources information via the \ref updateSourceInfoCount() and \ref updateSourceInfo()
   methods.
   \li You can add/remove/change update source information via the \ref addUpdateSourceInfo(),
   \ref removeUpdateSource(), \ref setUpdateSourceAt() methods.

   The class emits appropriate signals to inform listeners about changes in the update application.
*/

/*! \enum UpdateSourcesInfo::Error
 * Error codes related to retrieving update sources
 */

/*! \var UpdateSourcesInfo::Error UpdateSourcesInfo::NoError
 * No error occurred
 */

/*! \var UpdateSourcesInfo::Error UpdateSourcesInfo::NotYetReadError
 * The package information was not parsed yet from the XML file
 */

/*! \var UpdateSourcesInfo::Error UpdateSourcesInfo::CouldNotReadSourceFileError
 * the specified update source file could not be read (does not exist or not readable)
 */

/*! \var UpdateSourcesInfo::Error UpdateSourcesInfo::InvalidXmlError
 * The source file contains invalid XML. 
 */

/*! \var UpdateSourcesInfo::Error UpdateSourcesInfo::InvalidContentError
 * The source file contains valid XML, but does not match the expected format for source descriptions
 */

/*! \var UpdateSourcesInfo::Error UpdateSourcesInfo::CouldNotSaveChangesError
 * Changes made to the object could be saved back to the source file
 */

using namespace KDUpdater;

struct UpdateSourceInfoPriorityHigherThan
{
    bool operator()(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs) const
    {
        return lhs.priority > rhs.priority;
    }
};


struct UpdateSourcesInfo::UpdateSourcesInfoData
{
    UpdateSourcesInfoData(UpdateSourcesInfo *qq) :
        q(qq),
        error(UpdateSourcesInfo::NotYetReadError),
        application(0),
        modified(false)
    {}

    UpdateSourcesInfo *q;

    QString errorMessage;
    UpdateSourcesInfo::Error error;
    Application *application;
    bool modified;
    QString fileName;
    QList<UpdateSourceInfo> updateSourceInfoList;

    void addUpdateSourceFrom(const QDomElement &element);
    void addChildElement(QDomDocument &doc, QDomElement &parentE, const QString &tagName, const QString &text, bool htmlText = false);
    void setInvalidContentError(const QString &detail);
    void clearError();
    void saveChanges();
};

void UpdateSourcesInfo::UpdateSourcesInfoData::setInvalidContentError(const QString &detail)
{
    error = UpdateSourcesInfo::InvalidContentError;
    errorMessage = tr("%1 contains invalid content: %2").arg(fileName, detail);
}

void UpdateSourcesInfo::UpdateSourcesInfoData::clearError()
{
    error = UpdateSourcesInfo::NoError;
    errorMessage.clear();
}

/*!
   \internal
*/
UpdateSourcesInfo::UpdateSourcesInfo(Application *application)
    : QObject(application),
      d(new UpdateSourcesInfo::UpdateSourcesInfoData(this))
{
    d->application = application;
}

/*!
   \internal
*/
UpdateSourcesInfo::~UpdateSourcesInfo()
{
    d->saveChanges();
    delete d;
}

/*!
   Returns a pointer to the update application for which this class manages update sources.
*/
Application *UpdateSourcesInfo::application() const
{
    return d->application;
}

/*!
   \internal
*/
bool UpdateSourcesInfo::isValid() const
{
    return d->error == NoError;
}

/*!
   returns a human-readable description of the error
 */
QString UpdateSourcesInfo::errorString() const
{
    return d->errorMessage;
}

/*!
   returns the last error
 */
UpdateSourcesInfo::Error UpdateSourcesInfo::error() const
{
    return d->error;
}

bool UpdateSourcesInfo::isModified() const
{
    return d->modified;
}

void UpdateSourcesInfo::setModified(bool modified)
{
    d->modified = modified;
}

/*!
   Sets the complete file name of the UpdateSources.xml file. The function also issues a call
   to refresh() to reload package information from the XML file.

   \sa KDUpdater::Application::setUpdateSourcesXMLFileName()
*/
void UpdateSourcesInfo::setFileName(const QString &fileName)
{
    if (d->fileName == fileName)
        return;

    d->fileName = fileName;
    refresh(); // load new file
}

/*!
   Returns the name of the UpdateSources.xml file that this class referred to.
*/
QString UpdateSourcesInfo::fileName() const
{
    return d->fileName;
}

/*!
   Returns the number of update source info structures contained in this class.
*/
int UpdateSourcesInfo::updateSourceInfoCount() const
{
    return d->updateSourceInfoList.count();
}

/*!
   Returns the update source info structure at \c index. If an invalid index is passed
   the function returns a dummy constructor.
*/
UpdateSourceInfo UpdateSourcesInfo::updateSourceInfo(int index) const
{
    if (index < 0 || index >= d->updateSourceInfoList.count())
        return UpdateSourceInfo();

    return d->updateSourceInfoList[index];
}

/*!
   Adds an update source info to this class. Upon successful addition, the class emits a
   \ref updateSourceInfoAdded() signal.
*/
void UpdateSourcesInfo::addUpdateSourceInfo(const UpdateSourceInfo &info)
{
    if (d->updateSourceInfoList.contains(info))
        return;
    d->updateSourceInfoList.push_back(info);
    qSort(d->updateSourceInfoList.begin(), d->updateSourceInfoList.end(), UpdateSourceInfoPriorityHigherThan());
    emit updateSourceInfoAdded(info);
    d->modified = true;
}

/*!
   Removes an update source info from this class. Upon successful removal, the class emits a
   \ref updateSourceInfoRemoved() signal.
*/
void UpdateSourcesInfo::removeUpdateSourceInfo(const UpdateSourceInfo &info)
{
    if (!d->updateSourceInfoList.contains(info))
        return;
    d->updateSourceInfoList.removeAll(info);
    emit updateSourceInfoRemoved(info);
    d->modified = true;
}

/*!
   Removes an update source info at \index in this class. Upon successful removal, the class emits a
   \ref updateSourceInfoRemoved() signal.
*/
void UpdateSourcesInfo::removeUpdateSourceInfoAt(int index)
{
    if (index < 0 || index >= d->updateSourceInfoList.count())
        return;
    UpdateSourceInfo info = d->updateSourceInfoList[index];
    d->updateSourceInfoList.removeAt(index);
    emit updateSourceInfoRemoved(info);
    d->modified = true;
}

/*!
   Changes the update source info at \c index to \c info. If \c index is equal to the number of
   source info structures in this class (\ref updateSourceInfoCount()) then \c info is appended;
   otherwise the existing info at \c index will be changed.

   Depending on what the function does \ref updateSourceInfoAdded() or \ref updateSourceInfoChanged()
   signal is emitted.
*/
void UpdateSourcesInfo::setUpdateSourceInfoAt(int index, const UpdateSourceInfo &info)
{
    if (index < 0 || index > d->updateSourceInfoList.count())
        return;

    if (index == d->updateSourceInfoList.count()) {
        d->updateSourceInfoList.append(info);
        emit updateSourceInfoAdded(info);
    } else {
        UpdateSourceInfo oldInfo = d->updateSourceInfoList[index];
        if (info == oldInfo)
            return;

        d->updateSourceInfoList[index] = info;
        emit updateSourceInfoChanged(info, oldInfo);
    }
    d->modified = true;
}

/*!
   This slot reloads the update source information from UpdateSources.xml.
*/
void UpdateSourcesInfo::refresh()
{
    d->saveChanges(); // save changes done in the previous file
    d->updateSourceInfoList.clear();

    QFile file(d->fileName);
    
    // if the file does not exist then we just skip the reading
    if (!file.exists()) {
        d->clearError();
        emit reset();
        return;
    }
    
    // Open the XML file
    if (!file.open(QFile::ReadOnly)) {
        d->errorMessage = tr("Could not read \"%1\"").arg(d->fileName);
        d->error = CouldNotReadSourceFileError;
        emit reset();
        return;
    }

    QDomDocument doc;
    QString parseErrorMessage;
    int parseErrorLine;
    int parseErrorColumn;
    if (!doc.setContent(&file, &parseErrorMessage, &parseErrorLine, &parseErrorColumn)) {
        d->error = InvalidXmlError;
        d->errorMessage = tr("XML Parse error in %1 at %2, %3: %4")
                          .arg(d->fileName,
                                QString::number(parseErrorLine),
                                QString::number(parseErrorColumn),
                                parseErrorMessage);
        emit reset();
        return;
    }

    // Now parse the XML file.
    QDomElement rootE = doc.documentElement();
    if (rootE.tagName() != QLatin1String("UpdateSources")) {
        d->setInvalidContentError(tr("Root element %1 unexpected, should be \"UpdateSources\"").arg(rootE.tagName()));
        emit reset();
        return;
    }

    QDomNodeList childNodes = rootE.childNodes();
    for (int i = 0; i < childNodes.count(); i++) {
        QDomNode childNode = childNodes.item(i);
        QDomElement childNodeE = childNode.toElement();
        if (childNodeE.isNull())
            continue;

        if (childNodeE.tagName() == QLatin1String("UpdateSource"))
            d->addUpdateSourceFrom(childNodeE);
    }

    d->clearError();
    emit reset();
}

void UpdateSourcesInfo::UpdateSourcesInfoData::saveChanges()
{
    if (!modified || fileName.isEmpty())
        return;

    const bool hadSaveError = error == UpdateSourcesInfo::CouldNotSaveChangesError; 
        
    QDomDocument doc;

    QDomElement rootE = doc.createElement(QLatin1String("UpdateSources"));
    doc.appendChild(rootE);

    for (int i = 0; i < updateSourceInfoList.count(); i++) {
        UpdateSourceInfo info = updateSourceInfoList.at(i);

        QDomElement infoE = doc.createElement(QLatin1String("UpdateSource"));
        rootE.appendChild(infoE);
        addChildElement(doc, infoE, QLatin1String("Name"), info.name);
        addChildElement(doc, infoE, QLatin1String("Title"), info.title);
        addChildElement(doc, infoE, QLatin1String("Description"),  info.description,
                           (info.description.length() && info.description.at(0) == QLatin1Char('<')));
        addChildElement(doc, infoE, QLatin1String("Url"),  info.url.toString());
    }

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly)) {
        error = UpdateSourcesInfo::CouldNotSaveChangesError;
        errorMessage = tr("Could not save changes to \"%1\": %2").arg(fileName, file.errorString());
        return;
    }
    
    QTextStream stream(&file);
    doc.save(stream, 2);
    stream.flush();
    file.close();

    if (file.error() != QFile::NoError) {
        error = UpdateSourcesInfo::CouldNotSaveChangesError;
        errorMessage = tr("Could not save changes to \"%1\": %2").arg(fileName, file.errorString());
        return;
    }
    
    //if there was a write error before, clear the error, as the write was successful now
    if (hadSaveError)
        clearError();
    
    modified = false;
}

void UpdateSourcesInfo::UpdateSourcesInfoData::addUpdateSourceFrom(const QDomElement &element)
{
    if (element.tagName() != QLatin1String("UpdateSource"))
        return;

    QDomNodeList childNodes = element.childNodes();
    if (!childNodes.count())
        return;

    UpdateSourceInfo info;

    for (int i = 0; i<childNodes.count(); i++) {
        QDomNode childNode = childNodes.item(i);
        QDomElement childNodeE = childNode.toElement();
        if (childNodeE.isNull())
            continue;

        if (childNodeE.tagName() == QLatin1String("Name"))
            info.name = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Title"))
            info.title = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Description"))
            info.description = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Url"))
            info.url = childNodeE.text();
    }

    this->updateSourceInfoList.append(info);
}

void UpdateSourcesInfo::UpdateSourcesInfoData::addChildElement(QDomDocument &doc, QDomElement &parentE, const QString &tagName, const QString &text, bool htmlText)
{
    QDomElement childE = doc.createElement(tagName);
    parentE.appendChild(childE);

    if (htmlText) {
        QDomCDATASection textE = doc.createCDATASection(text);
        childE.appendChild(textE);
    } else {
        QDomText textE = doc.createTextNode(text);
        childE.appendChild(textE);
    }
}

/*!
   \ingroup kdupdater
   \struct KDUpdater::UpdateSourceInfo kdupdaterupdatesourcesinfo.h KDUpdaterUpdateSourcesInfo
   \brief Describes a single update source

   An update source is a repository that contains updates applicable for the application.
   This structure describes a single update source in terms of name, title, description, url and priority.
*/

/*!
   \var QString KDUpdater::UpdateSourceInfo::name
*/

/*!
   \var QString KDUpdater::UpdateSourceInfo::title
*/

/*!
   \var QString KDUpdater::UpdateSourceInfo::description
*/

/*!
   \var QUrl KDUpdater::UpdateSourceInfo::url
*/

/*!
   \var QUrl KDUpdater::UpdateSourceInfo::priority
*/

namespace KDUpdater {

bool operator==(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs)
{
    return lhs.name == rhs.name && lhs.title == rhs.title
        && lhs.description == rhs.description && lhs.url == rhs.url;
}

} // namespace KDUpdater
