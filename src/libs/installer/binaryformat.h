/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef BINARYFORMAT_H
#define BINARYFORMAT_H

#include "range.h"
#include "qinstallerglobal.h"

#include <QFile>
#include <QList>

namespace QInstaller {

class INSTALLER_EXPORT Resource : public QIODevice
{
    Q_OBJECT
    Q_DISABLE_COPY(Resource)

public:
    explicit Resource(const QString &path);
    Resource(const QByteArray &name, const QString &path);

    explicit Resource(const QSharedPointer<QIODevice> &device);
    Resource(const QSharedPointer<QIODevice> &device, const Range<qint64> &segment);
    Resource(const QByteArray &name, const QSharedPointer<QIODevice> &device,
        const Range<qint64> &segment);
    ~Resource();

    bool open();
    void close();

    bool seek(qint64 pos);
    qint64 size() const;

    QByteArray name() const;
    void setName(const QByteArray &name);

    Range<qint64> segment() const { return m_segment; }

    void copyData(QFileDevice *out) { copyData(this, out); }
    static void copyData(Resource *archive, QFileDevice *out);

private:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);
    bool open(OpenMode mode) { return QIODevice::open(mode); }

private:
    QSharedPointer<QIODevice> m_device;
    Range<qint64> m_segment;
    QFile m_inputFile;
    QByteArray m_name;
    bool m_deviceOpened;
};


class INSTALLER_EXPORT ResourceCollection
{
    Q_DECLARE_TR_FUNCTIONS(ResourceCollection)

public:
    ResourceCollection() {}
    explicit ResourceCollection(const QByteArray &name);

    QByteArray name() const;
    void setName(const QByteArray &ba);

    Range<qint64> segment() const { return m_segment; }
    void setSegment(const Range<qint64> &segment) const { m_segment = segment; }

    void write(QFileDevice *dev, qint64 positionOffset) const;
    void read(const QSharedPointer<QFile> &dev, qint64 offset);

    QList<QSharedPointer<Resource> > resources() const;
    QSharedPointer<Resource> resourceByName(const QByteArray &name) const;

    void appendResource(const QSharedPointer<Resource> &resource);
    void appendResources(const QList<QSharedPointer<Resource> > &resources);

    bool operator<(const ResourceCollection &other) const;
    bool operator==(const ResourceCollection &other) const;

private:
    QByteArray m_name;
    mutable Range<qint64> m_segment;
    QList<QSharedPointer<Resource> > m_resources;
};


class INSTALLER_EXPORT ResourceCollectionManager
{
public:
    Range<qint64> write(QFileDevice *dev, qint64 offset) const;
    void read(const QSharedPointer<QFile> &dev, qint64 offset);

    void reset();
    int collectionCount() const;

    QList<ResourceCollection> collections() const;
    ResourceCollection collectionByName(const QByteArray &name) const;

    void removeCollection(const QByteArray &name);
    void insertCollection(const ResourceCollection &collection);

private:
    void writeIndexEntry(const ResourceCollection &coll, QFileDevice *dev) const;
    ResourceCollection readIndexEntry(const QSharedPointer<QFile> &dev, qint64 offset);

private:
    QHash<QByteArray, ResourceCollection> m_collections;
};

} // namespace QInstaller

#endif // BINARYFORMAT_H
