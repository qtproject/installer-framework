/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "localpackagehub.h"
#include "fileutils.h"
#include "globals.h"
#include "constants.h"

#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>

using namespace KDUpdater;
using namespace QInstaller;

/*!
    \inmodule kdupdater
    \class KDUpdater::LocalPackageHub
    \brief The LocalPackageHub class provides access to information about packages installed on the
        application side.

    This class parses the \e {installation information} XML file specified via the setFileName()
    method and provides access to the information defined within the file through an API. You can:
    \list
        \li Get the application name via the applicationName() method.
        \li Get the application version via the applicationVersion() method.
        \li Get information about the number of packages installed and their meta-data via the
            packageInfoCount() and packageInfo() methods.
    \endlist
*/

/*!
    \enum LocalPackageHub::Error
    Error codes related to retrieving information about installed packages:

    \value NoError                          No error occurred.
    \value NotYetReadError                  The installation information was not parsed yet from the
                                            XML file.
    \value CouldNotReadPackageFileError     The specified installation information file could not be
                                            read (does not exist or is not readable).
    \value InvalidXmlError                  The installation information file contains invalid XML.
    \value InvalidContentError              The installation information file contains valid XML, but
                                            does not match the expected format for package
                                            descriptions.
*/

struct LocalPackageHub::PackagesInfoData
{
    PackagesInfoData() :
        error(LocalPackageHub::NotYetReadError),
        modified(false)
    {}
    QString errorMessage;
    LocalPackageHub::Error error;
    QString fileName;
    QString applicationName;
    QString applicationVersion;
    bool modified;

    QMap<QString, LocalPackage> m_packageInfoMap;

    void addPackageFrom(const QDomElement &packageE);
    void setInvalidContentError(const QString &detail);
};

void LocalPackageHub::PackagesInfoData::setInvalidContentError(const QString &detail)
{
    error = LocalPackageHub::InvalidContentError;
    errorMessage = tr("%1 contains invalid content: %2").arg(fileName, detail);
}

/*!
    Constructs a local package hub. To fully setup the class you have to call setFileName().

    \sa setFileName
*/
LocalPackageHub::LocalPackageHub()
    : d(new PackagesInfoData())
{
}

/*!
    Destructor
*/
LocalPackageHub::~LocalPackageHub()
{
    writeToDisk();
    delete d;
}

/*!
    Returns \c true if LocalPackageHub is valid; otherwise returns \c false. You
    can use the errorString() method to receive a descriptive error message.
*/
bool LocalPackageHub::isValid() const
{
    return d->error <= NotYetReadError;
}

/*!
    Returns a map of all local installed packages. Map key is the package name.
*/
QMap<QString, LocalPackage> LocalPackageHub::localPackages() const
{
    return d->m_packageInfoMap;
}
/*!
    Returns a list of all local installed packages.
*/
QStringList LocalPackageHub::packageNames() const
{
    return d->m_packageInfoMap.keys();
}

/*!
    Returns a human-readable description of the last error that occurred.
*/
QString LocalPackageHub::errorString() const
{
    return d->errorMessage;
}

/*!
    Returns the error that was found during the processing of the installation information XML file.
    If no error was found, returns NoError.
*/
LocalPackageHub::Error LocalPackageHub::error() const
{
    return d->error;
}

/*!
    Sets the complete file name of the installation information XML file to \a fileName. The function
    also issues a call to refresh() to reload installation information from the XML file.
*/
void LocalPackageHub::setFileName(const QString &fileName)
{
    if (d->fileName == fileName)
        return;

    d->fileName = fileName;
    refresh();
}

/*!
    Returns the name of the installation information XML file that this class refers to.
*/
QString LocalPackageHub::fileName() const
{
    return d->fileName;
}

/*!
    Sets the application name to \a name. By default, this is the name specified in the
    \c <ApplicationName> element of the installation information XML file.
*/
void LocalPackageHub::setApplicationName(const QString &name)
{
    d->applicationName = name;
}

/*!
    Returns the application name.
*/
QString LocalPackageHub::applicationName() const
{
    return d->applicationName;
}

/*!
    Sets the application version to \a version. By default, this is the version specified in the
    \c <ApplicationVersion> element of the installation information XML file.
*/
void LocalPackageHub::setApplicationVersion(const QString &version)
{
    d->applicationVersion = version;
}

/*!
    Returns the application version.
*/
QString LocalPackageHub::applicationVersion() const
{
    return d->applicationVersion;
}

/*!
    Returns the number of KDUpdater::LocalPackage objects contained in this class.
*/
int LocalPackageHub::packageInfoCount() const
{
    return d->m_packageInfoMap.count();
}

/*!
    Returns the package info structure whose name is \a pkgName. If no such package was found, this
    function returns a \l{default-constructed value}.
*/
LocalPackage LocalPackageHub::packageInfo(const QString &pkgName) const
{
    return d->m_packageInfoMap.value(pkgName);
}

/*!
    Returns all package info structures.
*/
QList<LocalPackage> LocalPackageHub::packageInfos() const
{
    return d->m_packageInfoMap.values();
}

/*!
    Re-reads the installation information XML file and updates itself. Changes to applicationName()
    and applicationVersion() are lost after this function returns. The function emits a reset()
    signal after completion.
*/
void LocalPackageHub::refresh()
{
    // First clear internal variables
    d->applicationName.clear();
    d->applicationVersion.clear();
    d->m_packageInfoMap.clear();
    d->modified = false;

    QFile file(d->fileName);

    // if the file does not exist then we just skip the reading
    if (!file.exists()) {
        d->error = NotYetReadError;
        d->errorMessage = tr("The file %1 does not exist.").arg(d->fileName);
        return;
    }

    // Open Packages.xml
    if (!file.open(QFile::ReadOnly)) {
        d->error = CouldNotReadPackageFileError;
        d->errorMessage = tr("Cannot open %1.").arg(d->fileName);
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
        return;
    }
    file.close();

    // Now populate information from the XML file.
    QDomElement rootE = doc.documentElement();
    if (rootE.tagName() != QLatin1String("Packages")) {
        d->setInvalidContentError(tr("Root element %1 unexpected, should be 'Packages'.")
            .arg(rootE.tagName()));
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
    }

    d->error = NoError;
    d->errorMessage.clear();
}

/*!
    Marks the package specified by \a name as installed. Sets the values of
    \a version,
    \a title,
    \a treeName,
    \a description,
    \a sortingPriority,
    \a dependencies,
    \a autoDependencies,
    \a forcedInstallation,
    \a virtualComp,
    \a uncompressedSize,
    \a inheritVersionFrom,
    \a checkable,
    \a expandedByDefault,
    and \a contentSha1 for the package.
*/
void LocalPackageHub::addPackage(const QString &name,
                                 const QString &version,
                                 const QString &title,
                                 const QPair<QString, bool> &treeName,
                                 const QString &description,
                                 const int sortingPriority,
                                 const QStringList &dependencies,
                                 const QStringList &autoDependencies,
                                 bool forcedInstallation,
                                 bool virtualComp,
                                 quint64 uncompressedSize,
                                 const QString &inheritVersionFrom,
                                 bool checkable,
                                 bool expandedByDefault,
                                 const QString &contentSha1)
{
    // TODO: This somewhat unexpected, remove?
    if (d->m_packageInfoMap.contains(name)) {
        // TODO: What about the other fields, update?
        d->m_packageInfoMap[name].version = version;
        d->m_packageInfoMap[name].lastUpdateDate = QDate::currentDate();
    } else {
        LocalPackage info;
        info.name = name;
        info.version = version;
        info.inheritVersionFrom = inheritVersionFrom;
        info.installDate = QDate::currentDate();
        info.title = title;
        info.treeName = treeName;
        info.description = description;
        info.sortingPriority = sortingPriority;
        info.dependencies = dependencies;
        info.autoDependencies = autoDependencies;
        info.forcedInstallation = forcedInstallation;
        info.virtualComp = virtualComp;
        info.uncompressedSize = uncompressedSize;
        info.checkable = checkable;
        info.expandedByDefault = expandedByDefault;
        info.contentSha1 = contentSha1;
        d->m_packageInfoMap.insert(name, info);
    }
    d->modified = true;
}

/*!
    Removes the package specified by \a name. Returns \c false if the package is not found.
*/
bool LocalPackageHub::removePackage(const QString &name)
{
    if (d->m_packageInfoMap.remove(name) <= 0)
        return false;

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

/*!
    Writes the installation information file to disk.
*/
void LocalPackageHub::writeToDisk()
{
    if (d->modified && (!d->m_packageInfoMap.isEmpty() || QFile::exists(d->fileName))) {
        QDomDocument doc;
        QDomElement root = doc.createElement(QLatin1String("Packages")) ;
        doc.appendChild(root);

        addTextChildHelper(&root, QLatin1String("ApplicationName"), d->applicationName);
        addTextChildHelper(&root, QLatin1String("ApplicationVersion"), d->applicationVersion);

        Q_FOREACH (const LocalPackage &info, d->m_packageInfoMap) {
            QDomElement package = doc.createElement(QLatin1String("Package"));

            addTextChildHelper(&package, QLatin1String("Name"), info.name);
            addTextChildHelper(&package, QLatin1String("Title"), info.title);
            addTextChildHelper(&package, QLatin1String("Description"), info.description);
            addTextChildHelper(&package, QLatin1String("SortingPriority"), QString::number(info.sortingPriority));
            addTextChildHelper(&package, scTreeName, info.treeName.first, QLatin1String("moveChildren"),
                               QVariant(info.treeName.second).toString());
            if (info.inheritVersionFrom.isEmpty())
                addTextChildHelper(&package, QLatin1String("Version"), info.version);
            else
                addTextChildHelper(&package, QLatin1String("Version"), info.version,
                                   QLatin1String("inheritVersionFrom"), info.inheritVersionFrom);
            addTextChildHelper(&package, QLatin1String("LastUpdateDate"), info.lastUpdateDate
                .toString(Qt::ISODate));
            addTextChildHelper(&package, QLatin1String("InstallDate"), info.installDate
                .toString(Qt::ISODate));
            addTextChildHelper(&package, QLatin1String("Size"),
                QString::number(info.uncompressedSize));

            if (info.dependencies.count())
                addTextChildHelper(&package, scDependencies, info.dependencies.join(QLatin1String(",")));
            if (info.autoDependencies.count())
                addTextChildHelper(&package, scAutoDependOn, info.autoDependencies.join(QLatin1String(",")));
            if (info.forcedInstallation)
                addTextChildHelper(&package, QLatin1String("ForcedInstallation"), QLatin1String("true"));
            if (info.virtualComp)
                addTextChildHelper(&package, QLatin1String("Virtual"), QLatin1String("true"));
            if (info.checkable)
                addTextChildHelper(&package, QLatin1String("Checkable"), QLatin1String("true"));
            if (info.expandedByDefault)
                addTextChildHelper(&package, QLatin1String("ExpandedByDefault"), QLatin1String("true"));
            if (!info.contentSha1.isEmpty())
                addTextChildHelper(&package, scContentSha1, info.contentSha1);

            root.appendChild(package);
        }

        // Open Packages.xml
        QFile file(d->fileName);
        if (!file.open(QFile::WriteOnly))
            return;

        file.write(doc.toByteArray(4));
        file.close();

        // Write permissions for installation information file
        QInstaller::setDefaultFilePermissions(
            &file, DefaultFilePermissions::NonExecutable);

        d->modified = false;
    }
}

void LocalPackageHub::PackagesInfoData::addPackageFrom(const QDomElement &packageE)
{
    if (packageE.isNull())
        return;

    QDomNodeList childNodes = packageE.childNodes();
    if (childNodes.count() == 0)
        return;

    LocalPackage info;
    info.forcedInstallation = false;
    info.virtualComp = false;
    info.checkable = false;
    info.expandedByDefault = false;
    for (int i = 0; i < childNodes.count(); i++) {
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
        else if (childNodeE.tagName() == QLatin1String("SortingPriority"))
            info.sortingPriority = childNodeE.text().toInt();
        else if (childNodeE.tagName() == scTreeName) {
            info.treeName.first = childNodeE.text();
            info.treeName.second = QVariant(childNodeE.attribute(QLatin1String("moveChildren"))).toBool();
        } else if (childNodeE.tagName() == QLatin1String("Version")) {
            info.version = childNodeE.text();
            info.inheritVersionFrom = childNodeE.attribute(QLatin1String("inheritVersionFrom"));
        }
        else if (childNodeE.tagName() == QLatin1String("Virtual"))
            info.virtualComp = childNodeE.text().toLower() == QLatin1String("true") ? true : false;
        else if (childNodeE.tagName() == QLatin1String("Size"))
            info.uncompressedSize = childNodeE.text().toULongLong();
        else if (childNodeE.tagName() == QLatin1String("Dependencies")) {
            info.dependencies = childNodeE.text().split(QInstaller::commaRegExp(),
                Qt::SkipEmptyParts);
        } else if (childNodeE.tagName() == QLatin1String("AutoDependOn")) {
            info.autoDependencies = childNodeE.text().split(QInstaller::commaRegExp(),
                Qt::SkipEmptyParts);
        } else if (childNodeE.tagName() == QLatin1String("ForcedInstallation"))
            info.forcedInstallation = childNodeE.text().toLower() == QLatin1String( "true" ) ? true : false;
        else if (childNodeE.tagName() == QLatin1String("LastUpdateDate"))
            info.lastUpdateDate = QDate::fromString(childNodeE.text(), Qt::ISODate);
        else if (childNodeE.tagName() == QLatin1String("InstallDate"))
            info.installDate = QDate::fromString(childNodeE.text(), Qt::ISODate);
        else if (childNodeE.tagName() == QLatin1String("Checkable"))
            info.checkable = childNodeE.text().toLower() == QLatin1String("true") ? true : false;
        else if (childNodeE.tagName() == QLatin1String("ExpandedByDefault"))
            info.expandedByDefault = childNodeE.text().toLower() == QLatin1String("true") ? true : false;
        else if (childNodeE.tagName() == QLatin1String("ContentSha1"))
            info.contentSha1 = childNodeE.text();
    }
    m_packageInfoMap.insert(info.name, info);
}

/*!
    Clears the installed package list.
*/
void LocalPackageHub::clearPackageInfos()
{
    d->m_packageInfoMap.clear();
    d->modified = true;
}

/*!
    \inmodule kdupdater
    \class KDUpdater::LocalPackage
    \brief The LocalPackage class describes a single installed package in the application.

    This class contains information about a single installed package in the application. The
    information contained in this class corresponds to the information described by the <Package>
    XML element in the installation information XML file.
*/

/*!
    \variable LocalPackage::name
    \brief The name of the package.
*/

/*!
    \variable LocalPackage::title
*/

/*!
    \variable LocalPackage::description
*/

/*!
    \variable LocalPackage::version
*/

/*!
    \variable LocalPackage::lastUpdateDate
*/

/*!
    \variable LocalPackage::installDate
*/
