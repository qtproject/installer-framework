/**************************************************************************
**
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
**************************************************************************/

#include "metadata.h"

#include "constants.h"
#include "globals.h"
#include "metadatajob.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QByteArrayMatcher>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Metadata
    \brief The Metadata class represents fetched metadata from a repository.
*/


/*!
    \internal
*/
static bool verifyFileIntegrityFromElement(const QDomElement &element, const QString &childNodeName,
    const QString &attribute, const QString &metaDirectory, bool testChecksum)
{
    const QDomNodeList nodes = element.childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        const QDomNode node = nodes.at(i);
        if (node.nodeName() != childNodeName)
            continue;

        const QDir dir(metaDirectory);
        const QString filename = attribute.isEmpty()
            ? node.toElement().text()
            : node.toElement().attribute(attribute);

        if (filename.isEmpty())
            continue;

        QFile file(dir.absolutePath() + QDir::separator() + filename);
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(QInstaller::lcInstallerInstallLog)
                << "Cannot open" << file.fileName()
                << "for reading:" << file.errorString();
            return false;
        }

        if (!testChecksum)
            continue;

        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(&file);

        const QByteArray checksum = hash.result().toHex();
        if (!QFileInfo::exists(dir.absolutePath() + QDir::separator()
                + QString::fromLatin1(checksum) + QLatin1String(".sha1"))) {
            qCWarning(QInstaller::lcInstallerInstallLog)
                << "Unexpected checksum for file" << file.fileName();
            return false;
        }
    }
    return true;
}

/*!
    Constructs a new metadata object.
*/
Metadata::Metadata()
    : CacheableItem()
    , m_fromDefaultRepository(false)
{
}

/*!
    Constructs a new metadata object with a \a path.
*/
Metadata::Metadata(const QString &path)
    : CacheableItem(path)
    , m_fromDefaultRepository(false)
{
}

/*!
    Returns the checksum of this metadata which is the checksum of the Updates.xml file.
    The checksum value is stored to memory after first read, so a single object should
    not be reused for referring other metadata.
*/
QByteArray Metadata::checksum() const
{
    if (!m_checksum.isEmpty())
        return m_checksum;

    QFile updateFile(path() + QLatin1String("/Updates.xml"));
    if (!updateFile.open(QIODevice::ReadOnly))
        return QByteArray();

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&updateFile);
    m_checksum = hash.result().toHex();

    return m_checksum;
}

/*!
    Sets the checksum of this metadata to \a checksum. Calling this function
    will omit calculating the checksum from the update file when retrieving
    the checksum with \l{checksum()} for the first time.
*/
void Metadata::setChecksum(const QByteArray &checksum)
{
    m_checksum = checksum;
}

/*!
    Returns the root of the document tree representing the \c Updates.xml
    document of this metadata. Returns an empty \c QDomDocument in case
    of failure to reading the file.
*/
QDomDocument Metadata::updatesDocument() const
{
    QFile updateFile(path() + QLatin1String("/Updates.xml"));
    if (!updateFile.open(QIODevice::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open" << updateFile.fileName()
            << "for reading:" << updateFile.errorString();
        return QDomDocument();
    }

    QDomDocument doc;
    QString errorString;
    if (!doc.setContent(&updateFile, &errorString)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot set document content:" << errorString;
        return QDomDocument();
    }

    return doc;
}

/*!
    Returns \c true if the \c Updates.xml document of this metadata exists, and that all
    meta files referenced in the document exist. If the \c Updates.xml contains a \c Checksum
    element with a value of \c true, the integrity of the files is also verified.

    Returns \c false otherwise.
*/
bool Metadata::isValid() const
{
    QFile updateFile(path() + QLatin1String("/Updates.xml"));
    if (!updateFile.open(QIODevice::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open" << updateFile.fileName()
            << "for reading:" << updateFile.errorString();
        return false;
    }

    return verifyMetaFiles(&updateFile);
}

/*!
    Returns \c true if this metadata is active, \c false otherwise. Metadata is
    considered active if it is currently associated with a valid repository.
*/
bool Metadata::isActive() const
{
    return m_repository.isValid();
}

/*!
    Checks whether this metadata object obsoletes the \a other metadata. The other metadata
    is considered obsolete if it is not currently associated with any repository, and the
    URL of the calling metadata matches the last known URL of the other metadata. Returns
    \c true if the current metadata obsoletes the other, \c false otherwise.
*/
bool Metadata::obsoletes(CacheableItem *other)
{
    if (Metadata *meta = dynamic_cast<Metadata *>(other)) {
        // If the current metadata is not in use it should not replace anything.
        if (!isActive())
            return false;

        // If the other metadata is in use it is not obsolete.
        if (meta->isActive())
            return false;

        // Current metadata has the same persistent url as other metadata, other is obsolete.
        if (persistentRepositoryPath() == meta->persistentRepositoryPath())
            return true;

        // The refreshed url of the current metadata matches the persistent url of other
        // metadata, other is obsolete.
        if (m_repository.url().path(QUrl::FullyEncoded) == meta->persistentRepositoryPath())
            return true;
    }
    return false;
}

/*!
    Returns the repository object set for this metadata. This is the repository
    the metadata is currently associated with, which may be different from the
    repository where it was originally fetched.
*/
Repository Metadata::repository() const
{
    return m_repository;
}

/*!
    Sets the \a repository object of this metadata. If a repository
    is already set, the new one will override the previous one. The metadata becomes
    associated with the set repository even if it was fetched from another one.
*/
void Metadata::setRepository(const Repository &repository)
{
    m_repository = repository;
}

/*!
    Returns \c true if this metadata is available from a default repository,
    which means a repository without category, \c false otherwise.
*/
bool Metadata::isAvailableFromDefaultRepository() const
{
    return m_fromDefaultRepository;
}

/*!
    Sets the metadata available from a default repository based on the value
    of \a defaultRepository. This is not mutually exclusive from a metadata
    that has repository categories set.
*/
void Metadata::setAvailableFromDefaultRepository(bool defaultRepository)
{
    m_fromDefaultRepository = defaultRepository;
}

/*!
    Sets the repository path of this metadata from \a url, without the protocol or hostname.
    Unlike \l{setRepository()} this value is saved to disk, which allows retrieving the
    repository path of the metadata on later runs.
*/
void Metadata::setPersistentRepositoryPath(const QUrl &url)
{
    const QString newPath = url.path(QUrl::FullyEncoded).trimmed();
    if (m_persistentRepositoryPath == newPath)
        return;

    QFile file(path() + QLatin1String("/repository.txt"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open" << file.fileName() << "for writing:" << file.errorString();
        return;
    }
    QTextStream out(&file);
    out << newPath;

    m_persistentRepositoryPath = newPath;
}

/*!
    Returns the persistent repository path of the metadata.
*/
QString Metadata::persistentRepositoryPath()
{
    if (!m_persistentRepositoryPath.isEmpty())
        return m_persistentRepositoryPath;

    QFile file(path() + QLatin1String("/repository.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open" << file.fileName() << "for reading:" << file.errorString();
        return QString();
    }
    m_persistentRepositoryPath = QString::fromLatin1(file.readAll()).trimmed();
    return m_persistentRepositoryPath;
}

/*!
    Returns true if the updates document of this metadata contains the repository
    update element, which can include actions to \c add, \c remove, and \c replace
    repositories.

    \note This function does not check that the repository updates are actually
    valid, only that the updates document contains the \c RepositoryUpdate element.
*/
bool Metadata::containsRepositoryUpdates() const
{
    QFile updateFile(path() + QLatin1String("/Updates.xml"));
    if (!updateFile.open(QIODevice::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot open" << updateFile.fileName()
            << "for reading:" << updateFile.errorString();
        return false;
    }

    static const auto matcher = qMakeStaticByteArrayMatcher("<RepositoryUpdate>");
    while (!updateFile.atEnd()) {
        const QByteArray line = updateFile.readLine().simplified();
        if (matcher.indexIn(line) != -1)
            return true;
    }

    return false;
}

/*!
    Verifies that the files referenced in \a updateFile document exist
    on disk. If the document contains a \c Checksum element with a value
    of \c true, the integrity of the files is also verified.

    Returns \c true if the meta files are valid, \c false otherwise.
*/
bool Metadata::verifyMetaFiles(QFile *updateFile) const
{
    QDomDocument doc;
    QString errorString;
    if (!doc.setContent(updateFile, &errorString)) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot set document content:" << errorString;
        return false;
    }

    const QDomElement rootElement = doc.documentElement();
    const QDomNodeList childNodes = rootElement.childNodes();

    bool testChecksum = true;
    const QDomElement checksumElement = rootElement.firstChildElement(QLatin1String("Checksum"));
    if (!checksumElement.isNull())
        testChecksum = (checksumElement.text().toLower() == scTrue);

    for (int i = 0; i < childNodes.count(); ++i) {
        const QDomElement element = childNodes.at(i).toElement();
        if (element.isNull() || element.tagName() != QLatin1String("PackageUpdate"))
            continue;

        const QDomNodeList c2 = element.childNodes();
        QString packageName;
        QString unused1;
        QString unused2;

        // Only need the package name, so values for "online" and "testCheckSum" do not matter
        if (!MetadataJob::parsePackageUpdate(c2, packageName, unused1, unused2, true, true))
            continue; // nothing to check for this package

        const QString packagePath = QString::fromLatin1("%1/%2/").arg(path(), packageName);
        for (auto &metaTagName : scMetaElements) {
            const QDomElement metaElement = element.firstChildElement(metaTagName);
            if (metaElement.isNull())
                continue;

            if (metaElement.tagName() == QLatin1String("Licenses")) {
                if (!verifyFileIntegrityFromElement(metaElement, QLatin1String("License"),
                        QLatin1String("file"), packagePath, testChecksum)) {
                    return false;
                }
            } else if (metaElement.tagName() == QLatin1String("UserInterfaces")) {
                if (!verifyFileIntegrityFromElement(metaElement, QLatin1String("UserInterface"),
                        QString(), packagePath, testChecksum)) {
                    return false;
                }
            } else if (metaElement.tagName() == QLatin1String("Translations")) {
                if (!verifyFileIntegrityFromElement(metaElement, QLatin1String("Translation"),
                        QString(), packagePath, testChecksum)) {
                    return false;
                }
            } else if (metaElement.tagName() == QLatin1String("Script")) {
                if (!verifyFileIntegrityFromElement(metaElement.parentNode().toElement(),
                        QLatin1String("Script"), QString(), packagePath, testChecksum)) {
                    return false;
                }
            } else {
                Q_ASSERT_X(false, Q_FUNC_INFO, "Unknown meta element.");
            }
        }
    }

    return true;
}

} // namespace QInstaller
