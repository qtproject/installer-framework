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

#ifndef BINARYFORMAT_H
#define BINARYFORMAT_H

#include "installer_global.h"
#include "range.h"

#include <QCoreApplication>
#include <QtCore/private/qfsfileengine_p.h>
#include <QList>
#include <QSharedPointer>

namespace QInstaller {

struct OperationBlob {
    OperationBlob(const QString &n, const QString &x)
        : name(n), xml(x) {}
    QString name;
    QString xml;
};


class INSTALLER_EXPORT Resource : public QIODevice
{
    Q_OBJECT
    Q_DISABLE_COPY(Resource)

public:
    explicit Resource(const QString &path);
    Resource(const QString &path, const QByteArray &name);
    Resource(const QString &path, const Range<qint64> &segment);
    ~Resource();

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    bool open();
#else
    bool open(std::optional<QFile::Permissions> permissions = std::nullopt);
#endif
    void close() override;

    bool seek(qint64 pos) override;
    qint64 size() const override;

    QByteArray name() const;
    void setName(const QByteArray &name);

    Range<qint64> segment() const { return m_segment; }
    void setSegment(const Range<qint64> &segment) { m_segment = segment; }

    void copyData(QFileDevice *out) { copyData(this, out); }
    static void copyData(Resource *archive, QFileDevice *out);

private:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

    bool open(OpenMode mode) override { return QIODevice::open(mode); }
    void setOpenMode(OpenMode mode) { QIODevice::setOpenMode(mode); }

private:
    QFSFileEngine m_file;
    QByteArray m_name;
    Range<qint64> m_segment;
};


class INSTALLER_EXPORT ResourceCollection
{
    Q_DECLARE_TR_FUNCTIONS(ResourceCollection)

public:
    ResourceCollection();
    explicit ResourceCollection(const QByteArray &name);

    QByteArray name() const;
    void setName(const QByteArray &ba);

    QList<QSharedPointer<Resource> > resources() const;
    QSharedPointer<Resource> resourceByName(const QByteArray &name) const;

    void appendResource(const QSharedPointer<Resource> &resource);
    void appendResources(const QList<QSharedPointer<Resource> > &resources);

private:
    QByteArray m_name;
    QList<QSharedPointer<Resource> > m_resources;
};


class INSTALLER_EXPORT ResourceCollectionManager
{
    Q_DECLARE_TR_FUNCTIONS(ResourceCollectionManager)

public:
    void read(QFileDevice *dev, qint64 offset);
    Range<qint64> write(QFileDevice *dev, qint64 offset) const;

    void clear();
    int collectionCount() const;

    QList<ResourceCollection> collections() const;
    ResourceCollection collectionByName(const QByteArray &name) const;

    void removeCollection(const QByteArray &name);
    void insertCollection(const ResourceCollection &collection);

private:
    QHash<QByteArray, ResourceCollection> m_collections;
};

} // namespace QInstaller

#endif // BINARYFORMAT_H
