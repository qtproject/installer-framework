/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdupdaterpackagesinfo.h"
#include "globals.h"

#include <QFileInfo>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QVector>

using namespace KDUpdater;

/*!
    \inmodule kdupdater
    \class KDUpdater::PackagesInfo
    \brief The PackagesInfo class provides access to information about packages installed on the
        application side.

    This class parses the package information XML file specified via the setFileName() method and
    provides access to the information defined within the package information file through an easy
    to use API. You can:
    \list
        \li Get the application name via the applicationName() method.
        \li Get the application version via the applicationVersion() method.
        \li Get information about the number of packages installed and their meta-data via the
            packageInfoCount() and packageInfo() methods.
    \endlist

    Instances of this class cannot be created. Each instance of KDUpdater::Application has one
    instance of this class associated with it. You can fetch a pointer to an instance of this class
    for an application via the KDUpdater::Application::packagesInfo() method.
*/

/*!
    \enum PackagesInfo::Error
    Error codes related to retrieving package information.

    \value NoError                          No error occurred.
    \value NotYetReadError                  The package information was not parsed yet from the XML
                                            file.
    \value CouldNotReadPackageFileError     The specified package information file could not be
                                            read (does not exist or is not readable).
    \value InvalidXmlError                  The package information file contains invalid XML.
    \value InvalidContentError              The package information file contains valid XML, but
                                            does not match the expected format for package
                                            descriptions.
*/

struct PackagesInfo::PackagesInfoData
{
    PackagesInfoData() :
        error(PackagesInfo::NotYetReadError),
        modified(false)
    {}
    QString errorMessage;
    PackagesInfo::Error error;
    QString fileName;
    QString applicationName;
    QString applicationVersion;
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
PackagesInfo::PackagesInfo(QObject *parent)
    : QObject(parent),
      d(new PackagesInfoData())
{
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
    Returns \c true if PackagesInfo is valid; otherwise returns \c false. You
    can use the errorString() method to receive a descriptive error message.
*/
bool PackagesInfo::isValid() const
{
    if (!d->fileName.isEmpty())
        return d->error <= NotYetReadError;
    return d->error == NoError;
}

/*!
    Returns a human-readable description of the last error that occurred.
*/
QString PackagesInfo::errorString() const
{
    return d->errorMessage;
}

/*!
    Returns the error that was found during the processing of the package information XML file. If
    no error was found, returns NoError.
*/
PackagesInfo::Error PackagesInfo::error() const
{
    return d->error;
}

/*!
    Sets the complete file name of the package information XML file to \a fileName. The function
    also issues a call to refresh() to reload package information from the XML file.

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
    Returns the name of the package information XML file that this class refers to.
*/
QString PackagesInfo::fileName() const
{
    return d->fileName;
}

/*!
    Sets the application name to \a name. By default, this is the name specified in the
    ApplicationName XML element of the package information XML file.
*/
void PackagesInfo::setApplicationName(const QString &name)
{
    d->applicationName = name;
}

/*!
    Returns the application name.
*/
QString PackagesInfo::applicationName() const
{
    return d->applicationName;
}

/*!
    Sets the application version to \a version. By default, this is the version specified in the
    ApplicationVersion XML element of package information XML file.
*/
void PackagesInfo::setApplicationVersion(const QString &version)
{
    d->applicationVersion = version;
}

/*!
    Returns the application version.
*/
QString PackagesInfo::applicationVersion() const
{
    return d->applicationVersion;
}

/*!
    Returns the number of KDUpdater::PackageInfo objects contained in this class.
*/
int PackagesInfo::packageInfoCount() const
{
    return d->packageInfoList.count();
}

/*!
    Returns the package info structure at \a index. If an invalid index is passed, the
    function returns a \l{default-constructed value}.
*/
PackageInfo PackagesInfo::packageInfo(int index) const
{
    if (index < 0 || index >= d->packageInfoList.count())
        return PackageInfo();

    return d->packageInfoList.at(index);
}

/*!
    Returns the index of the package whose name is \a pkgName. If no such package was found, this
    function returns -1.
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
    Re-reads the package information XML file and updates itself. Changes to applicationName()
    and applicationVersion() are lost after this function returns. The function emits a reset()
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
    }

    d->error = NoError;
    d->errorMessage.clear();
    emit reset();
}

/*!
    Marks the package with \a name and \a version as installed.
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
    Updates the package and sets the package name to \a name, the version to \a version and the
    last update date to \a date.
*/
bool PackagesInfo::updatePackage(const QString &name, const QString &version, const QDate &date)
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
        else if (childNodeE.tagName() == QLatin1String("Dependencies")) {
            info.dependencies = childNodeE.text().split(QInstaller::commaRegExp(),
                QString::SkipEmptyParts);
        } else if (childNodeE.tagName() == QLatin1String("ForcedInstallation"))
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

    This signal is emitted whenever the contents of this class are refreshed, usually from within
    the refresh() slot.
*/

/*!
    \inmodule kdupdater
    \class KDUpdater::PackageInfo
    \brief The PackageInfo class describes a single installed package in the application.

    This class contains information about a single installed package in the application. The
    information contained in this class corresponds to the information described by the Package
    XML element in the package information XML file.
*/

/*!
    \variable PackageInfo::name
    \brief The name of the package.
*/

/*!
    \variable PackageInfo::pixmap
*/

/*!
    \variable PackageInfo::title
*/

/*!
    \variable PackageInfo::description
*/

/*!
    \variable PackageInfo::version
*/

/*!
    \variable PackageInfo::lastUpdateDate
*/

/*!
    \variable PackageInfo::installDate
*/
