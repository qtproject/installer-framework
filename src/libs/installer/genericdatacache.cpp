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

#include "genericdatacache.h"

#include "errors.h"
#include "fileutils.h"
#include "globals.h"
#include "metadata.h"
#include "updater.h"

#include <QDir>
#include <QDirIterator>
#include <QtConcurrent>

namespace QInstaller {

static const QLatin1String scManifestFile("manifest.json");

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::CacheableItem
    \brief The CacheableItem is a pure virtual class that defines an interface for
           a type suited for storage with the \l{GenericDataCache} class.
*/

/*!
    \fn QInstaller::CacheableItem::path() const

    Returns the path of this item. A subclass may override this method.
*/

/*!
    \fn QInstaller::CacheableItem::setPath(const QString &path)

    Sets the path of the item to \a path. A subclass may override this method.
*/

/*!
    \fn QInstaller::CacheableItem::checksum() const

    Returns the checksum of this item. A subclass must implement this method.
*/

/*!
    \fn QInstaller::CacheableItem::isValid() const

    Returns \c true if this item is valid, \c false otherwise.
    A subclass must implement this method.
*/

/*!
    \fn QInstaller::CacheableItem::isActive() const

    Returns \c true if this item is an actively used cache item, \c false otherwise.
    This information is used as a hint for filtering obsolete entries, an active item
    can never be obsolete.

    A subclass must implement this method.
*/

/*!
    \fn QInstaller::CacheableItem::obsoletes(CacheableItem *other)

    Returns \c true if the calling item obsoletes \a other item, \c false otherwise.
    This method is used for filtering obsolete entries from the cache.

    A subclass must implement this method.
*/

/*!
    Virtual destructor for \c CacheableItem.
*/
CacheableItem::~CacheableItem()
{
}


/*!
    \inmodule QtInstallerFramework
    \class QInstaller::GenericDataCache
    \brief The GenericDataCache is a template class for a checksum based storage
           of items on disk.

    GenericDataCache\<T\> manages a cache storage for a set \l{path()}, which contains
    a subdirectory for each registered item. An item of type \c T should implement
    methods declared in the \l{CacheableItem} interface. The GenericDataCache\<T\> class can
    still be explicitly specialized to use the derived type as a template argument, to
    allow retrieving items as the derived type without casting.

    Each cache has a manifest file in its root directory, which lists the version
    and wrapped type of the cache, and all its items. The file is updated automatically
    when the cache object is destructed, or it can be updated periodically by
    calling \l{sync()}.
*/

/*!
    \enum GenericDataCache::RegisterMode
    This enum holds the possible values for modes of registering items to cache.

    \value Copy
           The contents of the item are copied to the cache.
    \value Move
           The contents of the item are move to the cache.
*/

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::GenericDataCache()

    Constructs a new empty cache. The cache is invalid until set with a
    path and initialized.
*/
template <typename T>
GenericDataCache<T>::GenericDataCache()
    : m_version(QLatin1String("1.0.0"))
    , m_invalidated(true)
{
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::GenericDataCache(const QString &path, const QString &type, const QString &version)

    Constructs a cache to \a path with the given \a type and \a version.
    The cache is initialized automatically.
*/
template <typename T>
GenericDataCache<T>::GenericDataCache(const QString &path, const QString &type,
                                      const QString &version)
    : m_path(path)
    , m_type(type)
    , m_version(version)
    , m_invalidated(true)
{
    initialize();
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::~GenericDataCache()

    Deletes the cache object. Item contents on disk are kept.
*/
template <typename T>
GenericDataCache<T>::~GenericDataCache()
{
    if (m_invalidated)
        return;

    toDisk();
    invalidate();
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::setType(const QString &type)

    Sets the name of the wrapped type to \a type. This is used for determining if an
    existing cache holds items of the same type. Trying to load cached items with mismatching
    type results in discarding the old items. Optional.
*/
template<typename T>
void GenericDataCache<T>::setType(const QString &type)
{
    QMutexLocker _(&m_mutex);
    m_type = type;
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::setVersion(const QString &version)

    Sets the version of the cache to \a version. Loading from a cache with different
    expected version discards the old items. The version property defaults to \c{1.0.0}.
*/
template<typename T>
void GenericDataCache<T>::setVersion(const QString &version)
{
    QMutexLocker _(&m_mutex);
    m_version = version;
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::initialize()

    Initializes a cache. Creates a new directory for the path configured for
    this cache if it does not exist, and loads any previously cached items from
    the directory. The cache directory is locked for access by this process only.
    Returns \c true on success, \c false otherwise.
*/
template <typename T>
bool GenericDataCache<T>::initialize()
{
    QMutexLocker _(&m_mutex);
    Q_ASSERT(m_items.isEmpty());

    if (m_path.isEmpty()) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot initialize cache with empty path."));
        return false;
    }

    QDir directory(m_path);
    if (!directory.exists() && !directory.mkpath(m_path)) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot create directory \"%1\" for cache.").arg(m_path));
        return false;
    }

    if (m_lock && !m_lock->unlock()) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot initialize cache: %1").arg(m_lock->errorString()));
        return false;
    }

    m_lock.reset(new KDUpdater::LockFile(m_path + QLatin1String("/cache.lock")));
    if (!m_lock->lock()) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot initialize cache: %1").arg(m_lock->errorString()));
        return false;
    }

    if (!fromDisk())
        return false;

    m_invalidated = false;
    return true;
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::clear()

    Removes all items from the cache and deletes their contents on disk. If
    the cache directory becomes empty, it is also deleted. The cache becomes
    invalid after this action, even in case of error while clearing. In that
    case already deleted items will be lost. Returns \c true on success,
    \c false otherwise.
*/
template <typename T>
bool GenericDataCache<T>::clear()
{
    QMutexLocker _(&m_mutex);
    if (m_invalidated) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot clear invalidated cache."));
        return false;
    }

    QFile manifestFile(m_path + QDir::separator() + scManifestFile);
    if (manifestFile.exists() && !manifestFile.remove()) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot remove manifest file: %1").arg(manifestFile.errorString()));
        invalidate();
        return false;
    }

    bool success = true;
    for (T *item : qAsConst(m_items)) {
        try {
            QInstaller::removeDirectory(item->path());
        } catch (const Error &e) {
            setErrorString(QCoreApplication::translate("GenericDataCache",
                "Error while clearing cache: %1").arg(e.message()));
            success = false;
        }
    }

    invalidate();
    QDir().rmdir(m_path);
    return success;
}

/*!
   \fn template <typename T> QInstaller::GenericDataCache<T>::sync()

    Synchronizes the contents of the cache to its manifest file. Returns \c true
    if the manifest file was updates successfully, \c false otherwise.
*/
template<typename T>
bool GenericDataCache<T>::sync()
{
    QMutexLocker _(&m_mutex);
    if (m_invalidated) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot synchronize invalidated cache."));
        return false;
    }
    return toDisk();
}

/*!
   \fn template <typename T> QInstaller::GenericDataCache<T>::isValid() const

   Returns \c true if the cache is valid, \c false otherwise. A cache is considered
   valid when it is initialized to a set path.
*/
template<typename T>
bool GenericDataCache<T>::isValid() const
{
    QMutexLocker _(&m_mutex);
    return !m_invalidated;
}

/*!
   \fn template <typename T> QInstaller::GenericDataCache<T>::errorString() const

   Returns a string representing the last error with the cache.
*/
template<typename T>
QString GenericDataCache<T>::errorString() const
{
    QMutexLocker _(&m_mutex);
    return m_error;
}

/*!
   \fn template <typename T> QInstaller::GenericDataCache<T>::path() const

    Returns the path of the cache on disk.
*/
template <typename T>
QString GenericDataCache<T>::path() const
{
    QMutexLocker _(&m_mutex);
    return m_path;
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::setPath(const QString &path)

    Sets a new \a path for the cache and invalidates current items. Saves the information
    of the old cache to its manifest file.
*/
template <typename T>
void GenericDataCache<T>::setPath(const QString &path)
{
    QMutexLocker _(&m_mutex);
    if (!m_invalidated)
        toDisk();

    m_path = path;
    invalidate();
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::items() const

    Returns a list of cached items.
*/
template <typename T>
QList<T *> GenericDataCache<T>::items() const
{
    QMutexLocker _(&m_mutex);
    if (m_invalidated) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot retrieve items from invalidated cache."));
        return QList<T *>();
    }
    return m_items.values();
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::itemByChecksum(const QByteArray &checksum) const

    Returns an item that matches the \a checksum or \c nullptr in case
    no such item is cached.
*/
template <typename T>
T *GenericDataCache<T>::itemByChecksum(const QByteArray &checksum) const
{
    QMutexLocker _(&m_mutex);
    if (m_invalidated) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot retrieve item from invalidated cache."));
        return nullptr;
    }
    return m_items.value(checksum);
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::itemByPath(const QString &path) const

    Returns an item from the \a path or \c nullptr in case no such item
    is cached. Depending on the size of the cache, this can be much
    slower than retrieving an item with \l{itemByChecksum()}.
*/
template <typename T>
T *GenericDataCache<T>::itemByPath(const QString &path) const
{
    QMutexLocker _(&m_mutex);
    auto it = std::find_if(m_items.constBegin(), m_items.constEnd(),
        [&](T *item) {
            return (QDir::fromNativeSeparators(path) == QDir::fromNativeSeparators(item->path()));
        }
    );
    if (it != m_items.constEnd())
        return it.value();

    return nullptr;
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::registerItem(T *item, bool replace, RegisterMode mode)

    Registers the \a item to the cache. If \a replace is set to \c true,
    the new \a item replaces a previous item with the same checksum.

    The cache takes ownership of the object pointed by \a item. The contents of the
    item are copied or moved to the cache with a subdirectory name that matches the checksum
    of the item. The \a mode decides how the contents of the item are registered, either by
    copying or moving.

    Returns \c true on success or \c false if the item could not be registered.
*/
template <typename T>
bool GenericDataCache<T>::registerItem(T *item, bool replace, RegisterMode mode)
{
    QMutexLocker _(&m_mutex);
    if (m_invalidated) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot register item to invalidated cache."));
        return false;
    }
    if (!item) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot register null item."));
        return false;
    }
    if (!item->isValid()) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot register invalid item with checksum %1").arg(QLatin1String(item->checksum())));
        return false;
    }
    if (m_items.contains(item->checksum())) {
        if (replace) {// replace existing item including contents on disk
            remove(item->checksum());
        } else {
            setErrorString(QCoreApplication::translate("GenericDataCache",
                "Cannot register item with checksum %1. An item with the same checksum "
                "already exists in cache.").arg(QLatin1String(item->checksum())));
            return false;
        }
    }

    const QString newPath = m_path + QDir::separator() + QString::fromLatin1(item->checksum());
    try {
        // A directory is in the way but it isn't registered to the current cache, remove.
        QDir dir;
        if (dir.exists(newPath))
            QInstaller::removeDirectory(newPath);

        switch (mode) {
        case Copy:
            QInstaller::copyDirectoryContents(item->path(), newPath);
            break;
        case Move:
            // First, try moving the top level directory
            if (!dir.rename(item->path(), newPath)) {
                qCDebug(lcDeveloperBuild) << "Failed to rename directory" << item->path()
                    << "to" << newPath << ". Trying again.";
                // If that does not work, fallback to moving the contents one by one
                QInstaller::moveDirectoryContents(item->path(), newPath);
            }
            break;
        default:
            throw Error(QCoreApplication::translate("GenericDataCache",
                "Unknown register mode selected!"));
        }
    } catch (const Error &e) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Error while copying item to path \"%1\": %2").arg(newPath, e.message()));
        return false;
    }

    item->setPath(newPath);
    if (item->isValid()) {
        m_items.insert(item->checksum(), item);
        return true;
    }
    return false;
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::removeItem(const QByteArray &checksum)

    Removes the item specified by \a checksum from the cache and deletes the
    contents of the item from disk. Returns \c true if the item
    was removed successfully, \c false otherwise.
*/
template <typename T>
bool GenericDataCache<T>::removeItem(const QByteArray &checksum)
{
    QMutexLocker _(&m_mutex);
    return remove(checksum);
}

/*!
    \fn template <typename T> QInstaller::GenericDataCache<T>::obsoleteItems() const

    Returns items considered obsolete from the cache.
*/
template<typename T>
QList<T *> GenericDataCache<T>::obsoleteItems() const
{
    QMutexLocker _(&m_mutex);
    const QList<T *> obsoletes = QtConcurrent::blockingFiltered(m_items.values(),
        [&](T *item1) {
            if (item1->isActive()) // We can skip the iteration for active entries
                return false;

            for (T *item2 : qAsConst(m_items)) {
                if (item2->obsoletes(item1))
                    return true;
            }
            return false;
        }
    );
    return obsoletes;
}

/*!
   \internal

    Marks the cache invalid and clears all items. The contents
    on disk are not deleted. Releases the lock file of the cache.
*/
template <typename T>
void GenericDataCache<T>::invalidate()
{
    if (!m_items.isEmpty()) {
        qDeleteAll(m_items);
        m_items.clear();
    }
    if (m_lock && !m_lock->unlock()) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Error while invalidating cache: %1").arg(m_lock->errorString()));
    }
    m_invalidated = true;
}

/*!
    \internal

    Sets the current error string to \a error and prints it as a warning to the console.
*/
template <typename T>
void GenericDataCache<T>::setErrorString(const QString &error) const
{
    m_error = error;
    qCWarning(QInstaller::lcInstallerInstallLog) << error;
}

/*!
    \internal

    Reads the manifest file of the cache if one exists, and populates the internal
    hash from the file contents. Returns \c true if the manifests was read successfully
    or if the reading was omitted. This is the case if the file does not exist yet, or
    the type or version of the manifest does not match the current cache object. In case
    of mismatch the old items are not restored. Returns \c false otherwise.
*/
template<typename T>
bool GenericDataCache<T>::fromDisk()
{
    QFile manifestFile(m_path + QDir::separator() + scManifestFile);
    if (!manifestFile.exists()) // Not yet created
        return true;

    if (!manifestFile.open(QIODevice::ReadOnly)) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot open manifest file: %1").arg(manifestFile.errorString()));
        return false;
    }

    const QByteArray manifestData = manifestFile.readAll();
    const QJsonDocument manifestJsonDoc(QJsonDocument::fromJson(manifestData));
    const QJsonObject docJsonObject = manifestJsonDoc.object();

    const QJsonValue type = docJsonObject.value(QLatin1String("type"));
    if (type.toString() != m_type) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Discarding existing items from cache of type:"
            << type.toString() << ". New type:" << m_type;
        return true;
    }

    const QJsonValue version = docJsonObject.value(QLatin1String("version"));
    if (KDUpdater::compareVersion(version.toString(), m_version) != 0) {
        qCDebug(QInstaller::lcInstallerInstallLog) << "Discarding existing items from cache with version:"
            << version.toString() << ". New version:" << m_version;
        return true;
    }

    const QJsonArray itemsJsonArray = docJsonObject.value(QLatin1String("items")).toArray();
    for (const auto &itemJsonValue : itemsJsonArray) {
        const QString checksum = itemJsonValue.toString();

        std::unique_ptr<T> item(new T(m_path + QDir::separator() + checksum));
        m_items.insert(checksum.toLatin1(), item.release());

        // The cache directory may contain other entries (unrelated directories or
        // invalid old cache items) which we don't care about, unless registering
        // a new entry requires overwriting them.
    }
    return true;
}

/*!
    \internal

    Writes the manifest file with the contents of the internal item hash.
    Returns \c true on success, \c false otherwise.
*/
template<typename T>
bool GenericDataCache<T>::toDisk()
{
    QFile manifestFile(m_path + QDir::separator() + scManifestFile);
    if (!manifestFile.open(QIODevice::WriteOnly)) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot open manifest file: %1").arg(manifestFile.errorString()));
        return false;
    }

    QJsonArray itemsJsonArray;
    const QList<QByteArray> keys = m_items.keys();
    for (const QByteArray &key : keys)
        itemsJsonArray.append(QJsonValue(QLatin1String(key)));

    QJsonObject docJsonObject;
    docJsonObject.insert(QLatin1String("items"), itemsJsonArray);
    docJsonObject.insert(QLatin1String("version"), m_version);
    if (!m_type.isEmpty())
        docJsonObject.insert(QLatin1String("type"), m_type);

    QJsonDocument manifestJsonDoc;
    manifestJsonDoc.setObject(docJsonObject);
    if (manifestFile.write(manifestJsonDoc.toJson()) == -1) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot write contents for manifest file: %1").arg(manifestFile.errorString()));
        return false;
    }
    return true;
}

/*!
    \internal
*/
template<typename T>
bool GenericDataCache<T>::remove(const QByteArray &checksum)
{
    if (m_invalidated) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot remove item from invalidated cache."));
        return false;
    }
    QScopedPointer<T> item(m_items.take(checksum));
    if (!item) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Cannot remove item specified by checksum %1: no such item exists.").arg(QLatin1String(checksum)));
        return false;
    }

    try {
        QInstaller::removeDirectory(item->path());
    } catch (const Error &e) {
        setErrorString(QCoreApplication::translate("GenericDataCache",
            "Error while removing directory \"%1\": %2").arg(item->path(), e.message()));
        return false;
    }
    return true;
}

template class GenericDataCache<Metadata>;

} // namespace QInstaller
