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

#include "kdupdaterpackagesinfo.h"
#include "kdupdaterapplication.h"

#include <QFileInfo>
#include <QDomDocument>
#include <QDomElement>
#include <QVector>

using namespace KDUpdater;

/*!
   \ingroup kdupdater
   \class KDUpdater::PackagesInfo kdupdaterpackagesinfo.h KDUpdaterPackagesInfo
   \brief Provides access to information about packages installed on the application side.

   This class parses the XML package file specified via the setFileName() method and
   provides access to the the information defined within the package file through an
   easy to use API. You can:
   \li get application name via the \ref applicationName() method
   \li get application version via the \ref applicationVersion() method
   \li get information about the number of packages installed and their meta-data via the
   \ref packageInfoCount() and \ref packageInfo() methods.

   Instances of this class cannot be created. Each instance of \ref KDUpdater::Application
   has one instance of this class associated with it. You can fetch a pointer to an instance
   of this class for an application via the \ref KDUpdater::Application::packagesInfo()
   method.
*/

/*! \enum UpdatePackagesInfo::Error
 * Error codes related to retrieving update sources
 */

/*! \var UpdatePackagesInfo::Error UpdatePackagesInfo::NoError
 * No error occurred
 */

/*! \var UpdatePackagesInfo::Error UpdatePackagesInfo::NotYetReadError
 * The package information was not parsed yet from the XML file
 */

/*! \var UpdatePackagesInfo::Error UpdatePackagesInfo::CouldNotReadPackageFileError
 * the specified update source file could not be read (does not exist or not readable)
 */

/*! \var UpdatePackagesInfo::Error UpdatePackagesInfo::InvalidXmlError
 * The source file contains invalid XML. 
 */

/*! \var UpdatePackagesInfo::Error UpdatePackagesInfo::InvalidContentError
 * The source file contains valid XML, but does not match the expected format for package descriptions
 */

struct PackagesInfo::PackagesInfoData
{
    PackagesInfoData() :
        application(0),
        error(PackagesInfo::NotYetReadError),
        compatLevel(-1),
        modified(false)
    {}
    Application *application;
    QString errorMessage;
    PackagesInfo::Error error;
    QString fileName;
    QString applicationName;
    QString applicationVersion;
    int compatLevel;
    bool modified;

    QVector<PackageInfo> packageInfoList;

    void addPackageFrom(const QDomElement &packageE);
    void setInvalidContentError(const QString &detail);
};

void PackagesInfo::PackagesInfoData::setInvalidContentError(const QString &detail)
{
    error = PackagesInfo::InvalidContentError;
    errorMessage = tr("%1 contains invalid content: %2").arg(fileName, detail);
}

/*!
   \internal
*/
PackagesInfo::PackagesInfo(Application *application)
    : QObject(application),
      d(new PackagesInfoData())
{
    d->application = application;
}

/*!
   \internal
*/
PackagesInfo::~PackagesInfo()
{
    writeToDisk();
    delete d;
}

/*!
   Returns a pointer to the application, whose package information this class provides
   access to.
*/
Application *PackagesInfo::application() const
{
    return d->application;
}

/*!
   Returns true if the PackagesInfo are valid else false is returned in which case
   the \a errorString() method can be used to receive a describing error message.
*/
bool PackagesInfo::isValid() const
{
    if (!d->fileName.isEmpty())
        return d->error <= NotYetReadError;
    return d->error == NoError;
}

/*!
   Returns a human-readable error message.
*/
QString PackagesInfo::errorString() const
{
    return d->errorMessage;
}

PackagesInfo::Error PackagesInfo::error() const
{
    return d->error;
}

/*!
   Sets the complete file name of the Packages.xml file. The function also issues a call to
   \ref refresh() to reload package information from the XML file.

   \sa KDUpdater::Application::setPackagesXMLFileName()
*/
void PackagesInfo::setFileName(const QString &fileName)
{
    if (d->fileName == fileName)
        return;

    d->fileName = fileName;
    refresh();
}

/*!
   Returns the name of the Packages.xml file that this class referred to.
*/
QString PackagesInfo::fileName() const
{
    return d->fileName;
}

/*!
   Sets the application name. By default this is the name specified in
   the ApplicationName XML element of the Packages.xml file.
*/
void PackagesInfo::setApplicationName(const QString &name)
{
    d->applicationName = name;
    d->modified = true;
}

/*!
   Returns the application name.
*/
QString PackagesInfo::applicationName() const
{
    return d->applicationName;
}

/*!
   Sets the application version. By default this is the version specified
   in the ApplicationVersion XML element of Packages.xml.
*/
void PackagesInfo::setApplicationVersion(const QString &version)
{
    d->applicationVersion = version;
    d->modified = true;
}

/*!
   Returns the application version.
*/
QString PackagesInfo::applicationVersion() const
{
    return d->applicationVersion;
}

/*!
   Returns the number of \ref KDUpdater::PackageInfo objects contained in this class.
*/
int PackagesInfo::packageInfoCount() const
{
    return d->packageInfoList.count();
}

/*!
   Returns the package info structure (\ref KDUpdater::PackageInfo) at index. If index is
   out of range then an empty package info structure is returned.
*/
PackageInfo PackagesInfo::packageInfo(int index) const
{
    if (index < 0 || index >= d->packageInfoList.count())
        return PackageInfo();

    return d->packageInfoList.at(index);
}

/*!
   Returns the compat level of the application.
*/
int PackagesInfo::compatLevel() const
{
    return d->compatLevel;
}

/*!
   This function returns the index of the package whose name is \c pkgName. If no such
   package was found, this function returns -1.
*/
int PackagesInfo::findPackageInfo(const QString &pkgName) const
{
    for (int i = 0; i < d->packageInfoList.count(); i++) {
        if (d->packageInfoList[i].name == pkgName)
            return i;
    }

    return -1;
}

/*!
   Returns all package info structures.
*/
QVector<PackageInfo> PackagesInfo::packageInfos() const
{
    return d->packageInfoList;
}

/*!
   This function re-reads the Packages.xml file and updates itself. Changes to \ref applicationName()
   and \ref applicationVersion() are lost after this function returns. The function emits a reset()
   signal after completion.
*/
void PackagesInfo::refresh()
{
    // First clear internal variables
    d->applicationName.clear();
    d->applicationVersion.clear();
    d->packageInfoList.clear();
    d->modified = false;

    QFile file(d->fileName);
    
    // if the file does not exist then we just skip the reading
    if (!file.exists()) {
        d->error = NotYetReadError;
        d->errorMessage = tr("The file %1 does not exist.").arg(d->fileName);
        emit reset();
        return;
    }

    // Open Packages.xml
    if (!file.open(QFile::ReadOnly)) {
        d->error = CouldNotReadPackageFileError;
        d->errorMessage = tr("Could not open %1.").arg(d->fileName);
        emit reset();
        return;
    }

    // Parse the XML document
    QDomDocument doc;
    QString parseErrorMessage;
    int parseErrorLine;
    int parseErrorColumn;
    if (!doc.setContent(&file, &parseErrorMessage, &parseErrorLine, &parseErrorColumn)) {
        d->error = InvalidXmlError;
        d->errorMessage = tr("Parse error in %1 at %2, %3: %4")
                          .arg(d->fileName,
                               QString::number(parseErrorLine),
                               QString::number(parseErrorColumn),
                               parseErrorMessage);
        emit reset();
        return;
    }
    file.close();

    // Now populate information from the XML file.
    QDomElement rootE = doc.documentElement();
    if (rootE.tagName() != QLatin1String("Packages")) {
        d->setInvalidContentError(tr("Root element %1 unexpected, should be 'Packages'.").arg(rootE.tagName()));
        emit reset();
        return;
    }

    QDomNodeList childNodes = rootE.childNodes();
    for (int i = 0; i < childNodes.count(); i++) {
        QDomNode childNode = childNodes.item(i);
        QDomElement childNodeE = childNode.toElement();
        if (childNodeE.isNull())
            continue;

        if (childNodeE.tagName() == QLatin1String("ApplicationName"))
            d->applicationName = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("ApplicationVersion"))
            d->applicationVersion = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Package"))
            d->addPackageFrom(childNodeE);
        else if (childNodeE.tagName() == QLatin1String("CompatLevel"))
            d->compatLevel = childNodeE.text().toInt();
    }

    d->error = NoError;
    d->errorMessage.clear();
    emit reset();
}

/*!
   Sets the application compat level.
*/
void PackagesInfo::setCompatLevel(int level)
{
    d->compatLevel = level;
    d->modified = true;
}

/*!
 Marks the package with \a name as installed in \a version.
 */
bool PackagesInfo::installPackage(const QString &name, const QString &version,
                                  const QString &title, const QString &description,
                                  const QStringList &dependencies, bool forcedInstallation,
                                  bool virtualComp, quint64 uncompressedSize,
                                  const QString &inheritVersionFrom)
{
    if (findPackageInfo(name) != -1)
        return updatePackage(name, version, QDate::currentDate());

    PackageInfo info;
    info.name = name;
    info.version = version;
    info.inheritVersionFrom = inheritVersionFrom;
    info.installDate = QDate::currentDate();
    info.title = title;
    info.description = description;
    info.dependencies = dependencies;
    info.forcedInstallation = forcedInstallation;
    info.virtualComp = virtualComp;
    info.uncompressedSize = uncompressedSize;
    d->packageInfoList.push_back(info);
    d->modified = true;
    return true;
}

/*!
   Update the package.
*/
bool PackagesInfo::updatePackage(const QString &name,
                                 const QString &version,
                                 const QDate &date)
{
    int index = findPackageInfo(name);

    if (index == -1)
        return false;

    d->packageInfoList[index].version = version;
    d->packageInfoList[index].lastUpdateDate = date;
    d->modified = true;
    return true;
}

/*!
 Remove the package with \a name.
 */
bool PackagesInfo::removePackage(const QString &name)
{
    const int index = findPackageInfo(name);
    if (index == -1)
        return false;

    d->packageInfoList.remove(index);
    d->modified = true;
    return true;
}

static void addTextChildHelper(QDomNode *node,
                               const QString &tag,
                               const QString &text,
                               const QString &attributeName = QString(),
                               const QString &attributeValue = QString())
{
    QDomElement domElement = node->ownerDocument().createElement(tag);
    QDomText domText = node->ownerDocument().createTextNode(text);

    domElement.appendChild(domText);
    if (!attributeName.isEmpty())
        domElement.setAttribute(attributeName, attributeValue);
    node->appendChild(domElement);
}

void PackagesInfo::writeToDisk()
{
    if (d->modified && (!d->packageInfoList.isEmpty() || QFile::exists(d->fileName))) {
        QDomDocument doc;
        QDomElement root = doc.createElement(QLatin1String("Packages")) ;
        doc.appendChild(root);

        addTextChildHelper(&root, QLatin1String("ApplicationName"), d->applicationName);
        addTextChildHelper(&root, QLatin1String("ApplicationVersion"), d->applicationVersion);
        if (d->compatLevel != -1)
            addTextChildHelper(&root, QLatin1String( "CompatLevel" ), QString::number(d->compatLevel));

        Q_FOREACH (const PackageInfo &info, d->packageInfoList) {
            QDomElement package = doc.createElement(QLatin1String("Package"));

            addTextChildHelper(&package, QLatin1String("Name"), info.name);
            addTextChildHelper(&package, QLatin1String("Pixmap"), info.pixmap);
            addTextChildHelper(&package, QLatin1String("Title"), info.title);
            addTextChildHelper(&package, QLatin1String("Description"), info.description);
            if (info.inheritVersionFrom.isEmpty())
                addTextChildHelper(&package, QLatin1String("Version"), info.version);
            else
                addTextChildHelper(&package, QLatin1String("Version"), info.version,
                                   QLatin1String("inheritVersionFrom"), info.inheritVersionFrom);
            addTextChildHelper(&package, QLatin1String("LastUpdateDate"), info.lastUpdateDate.toString(Qt::ISODate));
            addTextChildHelper(&package, QLatin1String("InstallDate"), info.installDate.toString(Qt::ISODate));
            addTextChildHelper(&package, QLatin1String("Size"), QString::number(info.uncompressedSize));
            QString assembledDependencies = QLatin1String("");
            Q_FOREACH (const QString & val, info.dependencies) {
                assembledDependencies += val + QLatin1String(",");
            }
            if (info.dependencies.count() > 0)
                assembledDependencies.chop(1);
            addTextChildHelper(&package, QLatin1String("Dependencies"), assembledDependencies);
            if (info.forcedInstallation)
                addTextChildHelper(&package, QLatin1String("ForcedInstallation"), QLatin1String("true"));
            if (info.virtualComp)
                addTextChildHelper(&package, QLatin1String("Virtual"), QLatin1String("true"));

            root.appendChild(package);
        }

        // Open Packages.xml
        QFile file(d->fileName);
        if (!file.open(QFile::WriteOnly))
            return;

        file.write(doc.toByteArray(4));
        file.close();
        d->modified = false;
    }
}

void PackagesInfo::PackagesInfoData::addPackageFrom(const QDomElement &packageE)
{
    if (packageE.isNull())
        return;

    QDomNodeList childNodes = packageE.childNodes();
    if (childNodes.count() == 0)
        return;

    PackageInfo info;
    info.forcedInstallation = false;
    info.virtualComp = false;
    for (int i = 0; i < childNodes.count(); i++) {
        QDomNode childNode = childNodes.item(i);
        QDomElement childNodeE = childNode.toElement();
        if (childNodeE.isNull())
            continue;

        if (childNodeE.tagName() == QLatin1String("Name"))
            info.name = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Pixmap"))
            info.pixmap = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Title"))
            info.title = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Description"))
            info.description = childNodeE.text();
        else if (childNodeE.tagName() == QLatin1String("Version")) {
            info.version = childNodeE.text();
            info.inheritVersionFrom = childNodeE.attribute(QLatin1String("inheritVersionFrom"));
        }
        else if (childNodeE.tagName() == QLatin1String("Virtual"))
            info.virtualComp = childNodeE.text().toLower() == QLatin1String("true") ? true : false;
        else if (childNodeE.tagName() == QLatin1String("Size"))
            info.uncompressedSize = childNodeE.text().toULongLong();
        else if (childNodeE.tagName() == QLatin1String("Dependencies"))
            info.dependencies = childNodeE.text().split(QRegExp(QLatin1String("\\b(,|, )\\b")), QString::SkipEmptyParts);
        else if (childNodeE.tagName() == QLatin1String("ForcedInstallation"))
            info.forcedInstallation = childNodeE.text().toLower() == QLatin1String( "true" ) ? true : false;
        else if (childNodeE.tagName() == QLatin1String("LastUpdateDate"))
            info.lastUpdateDate = QDate::fromString(childNodeE.text(), Qt::ISODate);
        else if (childNodeE.tagName() == QLatin1String("InstallDate"))
            info.installDate = QDate::fromString(childNodeE.text(), Qt::ISODate);
    }

    this->packageInfoList.append(info);
}

/*!
    Clears the installed package list.
*/
void PackagesInfo::clearPackageInfoList()
{
    d->packageInfoList.clear();
    d->modified = true;
    emit reset();
}

/*!
   \fn void KDUpdater::PackagesInfo::reset()

   This signal is emitted whenever the contents of this class is refreshed, usually from within
   the \ref refresh() slot.
*/

/*!
   \ingroup kdupdater
   \struct KDUpdater::PackageInfo kdupdaterpackagesinfo.h KDUpdaterPackageInfo
   \brief Describes a single installed package in the application.

   This structure contains information about a single installed package in the application.
   The information contained in this structure corresponds to the information described
   by the Package XML element in Packages.xml
*/

/*!
   \var QString KDUpdater::PackageInfo::name
*/

/*!
   \var QString KDUpdater::PackageInfo::pixmap
*/

/*!
   \var QString KDUpdater::PackageInfo::title
*/

/*!
   \var QString KDUpdater::PackageInfo::description
*/

/*!
   \var QString KDUpdater::PackageInfo::version
*/

/*!
   \var QDate KDUpdater::PackageInfo::lastUpdateDate
*/

/*!
   \var QDate KDUpdater::PackageInfo::installDate
*/
