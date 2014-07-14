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

#include "binaryformatenginehandler.h"
#include "range.h"
#include "qinstallerglobal.h"

#include <QFile>
#include <QHash>
#include <QSharedPointer>
#include <QVector>

namespace QInstallerCreator {

class INSTALLER_EXPORT Archive : public QIODevice
{
public:
    explicit Archive(const QString &path);
    Archive(const QByteArray &name, const QSharedPointer<QFile> &device, const Range<qint64> &segment);
    ~Archive();

    bool open(OpenMode mode);
    void close();

    bool seek(qint64 pos);
    qint64 size() const;

    QByteArray name() const;
    void setName(const QByteArray &name);

    void copyData(QFileDevice *out) { copyData(this, out); }
    static void copyData(Archive *archive, QFileDevice *out);

private:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    QSharedPointer<QFile> m_device;
    const Range<qint64> m_segment;
    QFile m_inputFile;
    QByteArray m_name;
};

class INSTALLER_EXPORT Component
{
    Q_DECLARE_TR_FUNCTIONS(Component)

public:
    static Component readFromIndexEntry(const QSharedPointer<QFile> &dev, qint64 offset);

    void writeIndexEntry(QFileDevice *dev, qint64 offset) const;

    void writeData(QFileDevice *dev, qint64 positionOffset) const;
    void readData(const QSharedPointer<QFile> &dev, qint64 offset);

    QByteArray name() const;
    void setName(const QByteArray &ba);

    void appendArchive(const QSharedPointer<Archive> &archive);
    QSharedPointer<Archive> archiveByName(const QByteArray &name) const;
    QVector< QSharedPointer<Archive> > archives() const;

    bool operator<(const Component &other) const;
    bool operator==(const Component &other) const;

private:
    QByteArray m_name;
    QVector<QSharedPointer<Archive> > m_archives;
    mutable Range<qint64> m_binarySegment;
    QString m_dataDirectory;
};


class INSTALLER_EXPORT ComponentIndex
{
public:
    ComponentIndex();
    static ComponentIndex read(const QSharedPointer<QFile> &dev, qint64 offset);
    void writeIndex(QFileDevice *dev, qint64 offset) const;
    void writeComponentData(QFileDevice *dev, qint64 offset) const;
    Component componentByName(const QByteArray &name) const;
    void insertComponent(const Component &name);
    void removeComponent(const QByteArray &name);
    QVector<Component> components() const;
    int componentCount() const;

private:
    QHash<QByteArray, Component> m_components;
};
}

namespace QInstaller {

static const qint64 MagicInstallerMarker = 0x12023233UL;
static const qint64 MagicUninstallerMarker = 0x12023234UL;

static const qint64 MagicUpdaterMarker = 0x12023235UL;
static const qint64 MagicPackageManagerMarker = 0x12023236UL;

// this cookie is put at the end of the file to determine whether we have data
static const quint64 MagicCookie = 0xc2630a1c99d668f8LL;
static const quint64 MagicCookieDat = 0xc2630a1c99d668f9LL;

qint64 INSTALLER_EXPORT findMagicCookie(QFile *file, quint64 magicCookie = MagicCookie);

struct BinaryLayout
{
    QVector<Range<qint64> > metadataResourceSegments;
    qint64 operationsStart;
    qint64 operationsEnd;
    qint64 resourceCount;
    qint64 dataBlockSize;
    qint64 magicMarker;
    quint64 magicCookie;
    qint64 indexSize;
    qint64 endOfData;
};

class INSTALLER_EXPORT BinaryContentPrivate : public QSharedData
{
public:
    BinaryContentPrivate();
    explicit BinaryContentPrivate(const QString &path);
    BinaryContentPrivate(const BinaryContentPrivate &other);
    ~BinaryContentPrivate();

    qint64 m_magicMarker;
    qint64 m_dataBlockStart;

    QSharedPointer<QFile> m_appBinary;
    QSharedPointer<QFile> m_binaryDataFile;

    QList<Operation *> m_performedOperations;
    QList<QPair<QString, QString> > m_performedOperationsData;

    QVector<QByteArray> m_resourceMappings;
    QVector<Range<qint64> > m_metadataResourceSegments;

    QInstallerCreator::ComponentIndex m_componentIndex;
    QInstallerCreator::BinaryFormatEngineHandler m_binaryFormatEngineHandler;
};

class INSTALLER_EXPORT BinaryContent
{
public:
    BinaryContent();
    BinaryContent(const BinaryContent &rhs);

    static BinaryContent readFromBinary(const QString &path);
    static BinaryContent readAndRegisterFromBinary(const QString &path);
    static BinaryLayout readBinaryLayout(QFile *const file, qint64 cookiePos);

    int registerPerformedOperations();
    OperationList performedOperations() const;

    qint64 magicMarker() const;
    int registerEmbeddedQResources();
    void registerAsDefaultQResource(const QString &path);
    QInstallerCreator::ComponentIndex componentIndex() const;

private:
    explicit BinaryContent(const QString &path);
    static void readBinaryData(BinaryContent &content, const QSharedPointer<QFile> &file,
        const BinaryLayout &layout);

private:
    QSharedDataPointer<BinaryContentPrivate> d;
};

}

#endif // BINARYFORMAT_H
