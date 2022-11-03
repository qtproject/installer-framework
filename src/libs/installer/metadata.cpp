/**************************************************************************
**
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
**************************************************************************/

#include "metadata.h"

#include "constants.h"
#include "globals.h"
#include "metadatajob.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QFile>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Metadata
    \brief The Metadata class represents fetched metadata from a repository.
*/

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
    Returns \c true if the \c Updates.xml document of this metadata
    exists and can be opened for reading, \c false otherwise.
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
    return true;
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

} // namespace QInstaller
