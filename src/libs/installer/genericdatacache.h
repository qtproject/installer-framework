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

#ifndef GENERICDATACACHE_H
#define GENERICDATACACHE_H

#include "installer_global.h"
#include "lockfile.h"

#include <QHash>
#include <QMutex>
#include <QScopedPointer>

namespace QInstaller {

class INSTALLER_EXPORT CacheableItem
{
public:
    CacheableItem() = default;
    explicit CacheableItem(const QString &path)
        : m_path(path)
    {}
    virtual ~CacheableItem() = 0;

    virtual QString path() const { return m_path; }
    virtual void setPath(const QString &path) { m_path = path; }

    virtual QByteArray checksum() const = 0;
    virtual bool isValid() const = 0;

    virtual bool isActive() const = 0;
    virtual bool obsoletes(CacheableItem *other) = 0;

private:
    QString m_path;
};

template <typename T>
class INSTALLER_EXPORT GenericDataCache
{
public:
    enum RegisterMode {
        Copy = 0,
        Move = 1
    };

    GenericDataCache();
    explicit GenericDataCache(const QString &path, const QString &type, const QString &version);
    virtual ~GenericDataCache();

    void setType(const QString &type);
    void setVersion(const QString &version);

    bool initialize();
    bool clear();
    bool sync();

    bool isValid() const;
    QString errorString() const;

    QString path() const;
    void setPath(const QString &path);

    QList<T *> items() const;
    T *itemByChecksum(const QByteArray &checksum) const;
    T *itemByPath(const QString &path) const;

    bool registerItem(T *item, bool replace = false, RegisterMode mode = Copy);
    bool removeItem(const QByteArray &checksum);

    QList<T *> obsoleteItems() const;

private:
    void invalidate();
    void setErrorString(const QString &error) const;

    bool fromDisk();
    bool toDisk();

    bool remove(const QByteArray &checksum);

private:
    QScopedPointer<KDUpdater::LockFile> m_lock;
    mutable QMutex m_mutex;

    QHash<QByteArray, T *> m_items;
    QString m_path;
    QString m_type;
    QString m_version;
    mutable QString m_error;

    bool m_invalidated;
};

} // namespace QInstaller

#endif // GENERICDATACACHE_H
