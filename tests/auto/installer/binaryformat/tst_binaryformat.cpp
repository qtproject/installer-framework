/**************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include <binarycontent.h>
#include <binaryformat.h>
#include <errors.h>
#include <fileio.h>
#include <kdupdaterupdateoperation.h>

#include <QTest>
#include <QTemporaryFile>

static const qint64 scTinySize = 72704LL;
static const qint64 scSmallSize = 524288LL;
static const qint64 scLargeSize = 2097152LL;

using namespace QInstaller;

struct Layout
{
    Range<qint64> metaResourceSegment;
    QVector<Range<qint64> > metaResourceSegments;

    qint64 operationsCount;
    Range<qint64> operationsSegment;

    qint64 collectionCount;
    Range<qint64> resourceCollectionIndexSegment;

    qint64 binaryContentSize;
    qint64 magicMarker;
    quint64 magicCookie;

    qint64 endOfBinary;
};

class TestOperation : public KDUpdater::UpdateOperation
{
public:
    TestOperation(const QString &name) { setName(name); }

    virtual void backup() {}
    virtual bool performOperation() { return true; }
    virtual bool undoOperation() { return true; }
    virtual bool testOperation() { return true; }
    virtual Operation *clone() const { return 0; }
};

class tst_BinaryFormat : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        TestOperation op(QLatin1String("Operation 1"));
        op.setValue(QLatin1String("key"), QLatin1String("Operation 1 value."));
        op.setArguments(QStringList() << QLatin1String("arg1") << QLatin1String("arg2"));
        m_operations.append(OperationBlob(op.name(), op.toXml().toString()));

        op = TestOperation(QLatin1String("Operation 2"));
        op.setValue(QLatin1String("key"), QLatin1String("Operation 2 value."));
        op.setArguments(QStringList() << QLatin1String("arg1") << QLatin1String("arg2"));
        m_operations.append(OperationBlob(op.name(), op.toXml().toString()));
    }

    void findMagicCookieSmallFile()
    {
        QTemporaryFile file;
        file.open();

        try {
            QInstaller::blockingWrite(&file, QByteArray(scSmallSize, '1'));
            QInstaller::appendInt64(&file, QInstaller::BinaryContent::MagicCookie);

            QCOMPARE(QInstaller::BinaryContent::findMagicCookie(&file,
                QInstaller::BinaryContent::MagicCookie), scSmallSize);
        } catch (const QInstaller::Error &error) {
            QFAIL(qPrintable(error.message()));
        } catch (...) {
            QFAIL("Unexpected error.");
        }
    }

    void findMagicCookieLargeFile()
    {
        QTemporaryFile file;
        file.open();

        try {
            QInstaller::blockingWrite(&file, QByteArray(scLargeSize, '1'));
            QInstaller::appendInt64(&file, QInstaller::BinaryContent::MagicCookie);
            QInstaller::blockingWrite(&file, QByteArray(scTinySize, '2'));

            QCOMPARE(QInstaller::BinaryContent::findMagicCookie(&file,
                QInstaller::BinaryContent::MagicCookie), scLargeSize);
        } catch (const QInstaller::Error &error) {
            QFAIL(qPrintable(error.message()));
        } catch (...) {
            QFAIL("Unexpected error.");
        }
    }

    void findMagicCookieWithError()
    {
        QTest::ignoreMessage(QtDebugMsg, "create Error-Exception: \"No marker found, stopped after 71.00 KiB.\" ");

        QTemporaryFile file;
        file.open();

        try {
            QInstaller::blockingWrite(&file, QByteArray(scTinySize, '1'));

            // throws
            QInstaller::BinaryContent::findMagicCookie(&file, QInstaller::BinaryContent::MagicCookie);
        } catch (const QInstaller::Error &error) {
            QCOMPARE(qPrintable(error.message()), "No marker found, stopped after 71.00 KiB.");
        } catch (...) {
            QFAIL("Unexpected error.");
        }
    }

    void writeBinaryContent()
    {
        QTemporaryFile binary;
        QInstaller::openForWrite(&binary);
        QInstaller::blockingWrite(&binary, QByteArray(scTinySize, '1'));

        Layout layout;
        layout.endOfBinary = binary.pos();
        layout.magicMarker = BinaryContent::MagicInstallerMarker;
        layout.magicCookie = BinaryContent::MagicCookie;

        qint64 start = binary.pos();    // write default resource (fake)
        QInstaller::blockingWrite(&binary, QByteArray("Default resource data."));
        qint64 end = binary.pos();
        layout.metaResourceSegments.append(Range<qint64>::fromStartAndEnd(start, end)
            .moved(-layout.endOfBinary));

        start = end;    // // write additional resource (fake)
        QInstaller::blockingWrite(&binary, QByteArray("Additional resource data."));
        end = binary.pos();
        layout.metaResourceSegments.append(Range<qint64>::fromStartAndEnd(start, end)
            .moved(-layout.endOfBinary));
        layout.metaResourceSegment = Range<qint64>::fromStartAndEnd(layout.metaResourceSegments.first()
            .start(), layout.metaResourceSegments.last().end());

        start = end;
        layout.operationsCount = m_operations.count();
        QInstaller::appendInt64(&binary, layout.operationsCount);
        foreach (const OperationBlob &operation, m_operations) {
            QInstaller::appendString(&binary, operation.name);
            QInstaller::appendString(&binary, operation.xml);
        }
        QInstaller::appendInt64(&binary, layout.operationsCount);
        end = binary.pos();
        layout.operationsSegment = Range<qint64>::fromStartAndEnd(start, end).moved(-layout
            .endOfBinary);

        QTemporaryFile data;
        QTemporaryFile data2;
        { // put into the scope to make the temporary file auto remove feature work

            ResourceCollectionManager manager;

            QInstaller::openForWrite(&data);
            QInstaller::blockingWrite(&data, QByteArray("Collection 1, Resource 1."));
            data.close();

            ResourceCollection collection;
            collection.setName(QByteArray("Collection 1"));

            QSharedPointer<Resource> resource(new Resource(data.fileName()));
            resource->setName("Resource 1");
            collection.appendResource(resource);
            manager.insertCollection(collection);

            QInstaller::openForWrite(&data2);
            QInstaller::blockingWrite(&data2, QByteArray("Collection 2, Resource 2."));
            data2.close();

            ResourceCollection collection2;
            collection2.setName(QByteArray("Collection 2"));

            QSharedPointer<Resource> resource2(new
                Resource(data2.fileName()));
            resource2->setName("Resource 2");
            collection2.appendResource(resource2);
            manager.insertCollection(collection2);

            layout.collectionCount = manager.collectionCount();
            layout.resourceCollectionIndexSegment = manager.write(&binary, -layout.endOfBinary)
                .moved(-layout.endOfBinary);

            resource->close();
            resource2->close();
        }

        QInstaller::appendInt64Range(&binary, layout.resourceCollectionIndexSegment);
        foreach (const Range<qint64> &segment, layout.metaResourceSegments)
            QInstaller::appendInt64Range(&binary, segment);
        QInstaller::appendInt64Range(&binary, layout.operationsSegment);
        QInstaller::appendInt64(&binary, layout.metaResourceSegments.count());

        layout.binaryContentSize = (binary.pos() + (3 * sizeof(qint64))) - layout.endOfBinary;

        QInstaller::appendInt64(&binary, layout.binaryContentSize);
        QInstaller::appendInt64(&binary, layout.magicMarker);
        QInstaller::appendInt64(&binary, layout.magicCookie);

        binary.close();
        binary.setAutoRemove(false);

        m_layout = layout;
        m_binary = binary.fileName();
    }

    void readBinaryContent()
    {
        const QSharedPointer<QFile> binary(new QFile(m_binary));
        QInstaller::openForRead(binary.data());
        QCOMPARE(QInstaller::retrieveData(binary.data(), scTinySize), QByteArray(scTinySize, '1'));

        Layout layout;
        layout.endOfBinary = binary->pos();
        QCOMPARE(layout.endOfBinary, m_layout.endOfBinary);

        const qint64 pos = BinaryContent::findMagicCookie(binary.data(), BinaryContent::MagicCookie);
        const qint64 endOfBinaryContent = pos + sizeof(qint64);

        binary->seek(endOfBinaryContent - (4 * sizeof(qint64)));

        qint64 metaSegmentsCount = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(metaSegmentsCount, m_layout.metaResourceSegments.count());

        const qint64 offsetCollectionIndexSegments = endOfBinaryContent
            - ((metaSegmentsCount * (2 * sizeof(qint64))) // minus the size of the meta segments
            + (8 * sizeof(qint64))); // meta count, offset/length component index, marker, cookie...

        binary->seek(offsetCollectionIndexSegments);

        layout.resourceCollectionIndexSegment = QInstaller::retrieveInt64Range(binary.data());
        QCOMPARE(layout.resourceCollectionIndexSegment, m_layout.resourceCollectionIndexSegment);

        for (int i = 0; i < metaSegmentsCount; ++i)
            layout.metaResourceSegments.append(QInstaller::retrieveInt64Range(binary.data()));
        layout.metaResourceSegment = Range<qint64>::fromStartAndEnd(layout.metaResourceSegments
            .first().start(), layout.metaResourceSegments.last().end());
        QCOMPARE(layout.metaResourceSegment, m_layout.metaResourceSegment);
        QCOMPARE(layout.metaResourceSegments, m_layout.metaResourceSegments);

        layout.operationsSegment = QInstaller::retrieveInt64Range(binary.data());
        QCOMPARE(layout.operationsSegment, m_layout.operationsSegment);

        QCOMPARE(metaSegmentsCount, QInstaller::retrieveInt64(binary.data()));

        layout.binaryContentSize = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(layout.binaryContentSize, m_layout.binaryContentSize);
        QCOMPARE(layout.endOfBinary, endOfBinaryContent - layout.binaryContentSize);

        layout.magicMarker = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(layout.magicMarker, m_layout.magicMarker);

        layout.magicCookie = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(layout.magicCookie, m_layout.magicCookie);

        binary->seek(layout.endOfBinary + layout.operationsSegment.start());

        layout.operationsCount = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(layout.operationsCount, m_layout.operationsCount);

        for (int i = 0; i < layout.operationsCount; ++i) {
            QCOMPARE(m_operations.at(i).name, QInstaller::retrieveString(binary.data()));
            QCOMPARE(m_operations.at(i).xml, QInstaller::retrieveString(binary.data()));
        }

        layout.operationsCount = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(layout.operationsCount, m_layout.operationsCount);

        layout.collectionCount = QInstaller::retrieveInt64(binary.data());
        QCOMPARE(layout.collectionCount, m_layout.collectionCount);

        binary->seek(layout.endOfBinary + layout.resourceCollectionIndexSegment.start());
        m_manager.read(binary, layout.endOfBinary);

        const QList<ResourceCollection> components = m_manager.collections();
        QCOMPARE(components.count(), m_layout.collectionCount);

        ResourceCollection component = m_manager.collectionByName("Collection 1");
        QCOMPARE(component.resources().count(), 1);

        QSharedPointer<Resource> resource(component.resourceByName("Resource 1"));
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 1, Resource 1."));
        resource->close();

        component = m_manager.collectionByName("Collection 2");
        QCOMPARE(component.resources().count(), 1);

        resource = component.resourceByName("Resource 2");
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 2, Resource 2."));
        resource->close();
    }

    void testWriteBinaryContentFunction()
    {
        QSharedPointer<QFile> existingBinary(new QFile(m_binary));
        QInstaller::openForRead(existingBinary.data());

        QSharedPointer<QFile> file(new QTemporaryFile);
        QInstaller::openForWrite(file.data());
        QInstaller::blockingWrite(file.data(), QByteArray(scTinySize, '1'));

        ResourceCollection resources;
        foreach (const Range<qint64> &segment, m_layout.metaResourceSegments) {
            resources.appendResource(QSharedPointer<Resource> (new Resource(existingBinary,
                segment.moved(m_layout.endOfBinary))));
        }

        QList<OperationBlob> operations;
        foreach (const OperationBlob &operation, m_operations)
            operations.append(operation);

        BinaryContent::writeBinaryContent(file, resources, operations, m_manager,
            m_layout.magicMarker, m_layout.magicCookie);
        file->close();
        existingBinary->close();

        QInstaller::openForRead(file.data());
        QInstaller::openForRead(existingBinary.data());
        QCOMPARE(file->readAll(), existingBinary->readAll());
    }

    void testReadBinaryContentFunction()
    {
        QSharedPointer<QFile> file(new QFile(m_binary));
        QInstaller::openForRead(file.data());

        qint64 magicMarker;
        ResourceCollection collection;
        QList<OperationBlob> operations;
        ResourceCollectionManager manager;
        BinaryContent::readBinaryContent(file, &collection, &operations, &manager, &magicMarker,
            m_layout.magicCookie);

        QCOMPARE(magicMarker, m_layout.magicMarker);
        QCOMPARE(collection.resources().count(), m_layout.metaResourceSegments.count());
        for (int i = 0; i < collection.resources().count(); ++i) {
            QCOMPARE(collection.resources().at(i)->segment(), m_layout.metaResourceSegments.at(i)
                .moved(m_layout.endOfBinary));
        }

        QCOMPARE(operations.count(), m_operations.count());
        for (int i = 0; i < operations.count(); ++i) {
            QCOMPARE(operations.at(i).name, m_operations.at(i).name);
            QCOMPARE(operations.at(i).xml, m_operations.at(i).xml);
        }

        QCOMPARE(manager.collectionCount(), m_manager.collectionCount());

        ResourceCollection component = manager.collectionByName("Collection 1");
        QCOMPARE(component.resources().count(), 1);

        QSharedPointer<Resource> resource(component.resourceByName("Resource 1"));
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 1, Resource 1."));
        resource->close();

        component = manager.collectionByName("Collection 2");
        QCOMPARE(component.resources().count(), 1);

        resource = component.resourceByName("Resource 2");
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 2, Resource 2."));
        resource->close();
    }

    void cleanupTestCase()
    {
        m_manager.reset();
        QFile::remove(m_binary);
    }

private:
    Layout m_layout;
    QString m_binary;
    QList<OperationBlob> m_operations;
    ResourceCollectionManager m_manager;
};

QTEST_MAIN(tst_BinaryFormat)

#include "tst_binaryformat.moc"
