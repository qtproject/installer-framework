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

#include "../shared/packagemanager.h"

#include <binarycontent.h>
#include <binaryformat.h>
#include <errors.h>
#include <fileio.h>
#include <updateoperation.h>

#include <QTest>
#include <QTemporaryFile>

static const qint64 scTinySize = 72704LL;
static const qint64 scSmallSize = 524288LL;
static const qint64 scLargeSize = 2097152LL;

using namespace QInstaller;

struct Layout : public QInstaller::BinaryLayout
{
    qint64 metaSegmentsCount;
    qint64 operationsCount;
    qint64 collectionCount;
};

class TestOperation : public KDUpdater::UpdateOperation
{
public:
    explicit TestOperation(const QString &name, PackageManagerCore *core = nullptr)
        : KDUpdater::UpdateOperation(core)
    { setName(name); }

    virtual void backup() {}
    virtual bool performOperation() { return true; }
    virtual bool undoOperation() { return true; }
    virtual bool testOperation() { return true; }
    virtual KDUpdater::UpdateOperation *clone() const { return 0; }
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
        QTemporaryFile file;
        file.open();

        try {
            QInstaller::blockingWrite(&file, QByteArray(scTinySize, '1'));

            // throws
            QInstaller::BinaryContent::findMagicCookie(&file, QInstaller::BinaryContent::MagicCookie);
        } catch (const QInstaller::Error &error) {
            QCOMPARE(qPrintable(error.message()), "No marker found, stopped after 71.00 KB.");
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
        layout.endOfExectuable = binary.pos();
        layout.magicMarker = BinaryContent::MagicInstallerMarker;
        layout.magicCookie = BinaryContent::MagicCookie;

        qint64 start = binary.pos();    // write default resource (fake)
        QInstaller::blockingWrite(&binary, QByteArray("Default resource data."));
        qint64 end = binary.pos();
        layout.metaResourceSegments.append(Range<qint64>::fromStartAndEnd(start, end));

        start = end;    // // write additional resource (fake)
        QInstaller::blockingWrite(&binary, QByteArray("Additional resource data."));
        end = binary.pos();
        layout.metaResourceSegments.append(Range<qint64>::fromStartAndEnd(start, end));
        layout.metaResourcesSegment = Range<qint64>::fromStartAndEnd(layout.metaResourceSegments
            .first().start(), layout.metaResourceSegments.last().end());
        layout.metaSegmentsCount = layout.metaResourceSegments.count();

        start = end;
        layout.operationsCount = m_operations.count();
        QInstaller::appendInt64(&binary, layout.operationsCount);
        foreach (const OperationBlob &operation, m_operations) {
            QInstaller::appendString(&binary, operation.name);
            QInstaller::appendString(&binary, operation.xml);
        }
        QInstaller::appendInt64(&binary, layout.operationsCount);
        end = binary.pos();
        layout.operationsSegment = Range<qint64>::fromStartAndEnd(start, end);

        QTemporaryFile data;
        QInstaller::openForWrite(&data);
        QInstaller::blockingWrite(&data, QByteArray("Collection 1, Resource 1."));
        data.close();

        QSharedPointer<Resource> resource(new Resource(data.fileName(), QByteArray("Resource 1")));
        ResourceCollection collection(QByteArray("Collection 1"));
        collection.appendResource(resource);

        QTemporaryFile data2;
        QInstaller::openForWrite(&data2);
        QInstaller::blockingWrite(&data2, QByteArray("Collection 2, Resource 2."));
        data2.close();

        QSharedPointer<Resource> resource2(new Resource(data2.fileName(), QByteArray("Resource 2")));
        ResourceCollection collection2(QByteArray("Collection 2"));
        collection2.appendResource(resource2);

        ResourceCollectionManager manager;
        manager.insertCollection(collection);
        manager.insertCollection(collection2);

        layout.collectionCount = manager.collectionCount();
        layout.resourceCollectionsSegment = manager.write(&binary, -layout.endOfExectuable);

        QInstaller::appendInt64Range(&binary, layout.resourceCollectionsSegment.moved(-layout
            .endOfExectuable));

        foreach (const Range<qint64> &segment, layout.metaResourceSegments)
            QInstaller::appendInt64Range(&binary, segment.moved(-layout.endOfExectuable));

        QInstaller::appendInt64Range(&binary, layout.operationsSegment.moved(-layout
            .endOfExectuable));

        QInstaller::appendInt64(&binary, layout.metaSegmentsCount);
        layout.binaryContentSize = (binary.pos() + (3 * sizeof(qint64))) - layout.endOfExectuable;

        QInstaller::appendInt64(&binary, layout.binaryContentSize);
        QInstaller::appendInt64(&binary, layout.magicMarker);
        QInstaller::appendInt64(&binary, layout.magicCookie);

        layout.endOfBinaryContent = binary.pos();

        binary.close();
        binary.setAutoRemove(false);

        m_layout = layout;
        m_binary = binary.fileName();
    }

    void readBinaryContent()
    {
        QFile binary(m_binary);
        QInstaller::openForRead(&binary);
        QCOMPARE(QInstaller::retrieveData(&binary, scTinySize), QByteArray(scTinySize, '1'));

        Layout layout;
        layout.endOfExectuable = binary.pos();
        QCOMPARE(layout.endOfExectuable, m_layout.endOfExectuable);

        const qint64 pos = BinaryContent::findMagicCookie(&binary, BinaryContent::MagicCookie);
        layout.endOfBinaryContent = pos + sizeof(qint64);
        QCOMPARE(layout.endOfBinaryContent, m_layout.endOfBinaryContent);

        binary.seek(layout.endOfBinaryContent - (4 * sizeof(qint64)));

        layout.metaSegmentsCount = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.metaSegmentsCount, m_layout.metaSegmentsCount);

        const qint64 offsetCollectionIndexSegments = layout.endOfBinaryContent
            - ((layout.metaSegmentsCount * (2 * sizeof(qint64))) // minus size of the meta segments
            + (8 * sizeof(qint64))); // meta count, offset/length collection index, marker, cookie...

        binary.seek(offsetCollectionIndexSegments);

        layout.resourceCollectionsSegment = QInstaller::retrieveInt64Range(&binary)
            .moved(layout.endOfExectuable);
        QCOMPARE(layout.resourceCollectionsSegment, m_layout.resourceCollectionsSegment);

        for (int i = 0; i < layout.metaSegmentsCount; ++i) {
            layout.metaResourceSegments.append(QInstaller::retrieveInt64Range(&binary)
                .moved(layout.endOfExectuable));
        }
        layout.metaResourcesSegment = Range<qint64>::fromStartAndEnd(layout.metaResourceSegments
            .first().start(), layout.metaResourceSegments.last().end());

        QCOMPARE(layout.metaResourcesSegment, m_layout.metaResourcesSegment);
        QCOMPARE(layout.metaResourceSegments.first(), m_layout.metaResourceSegments.first());
        QCOMPARE(layout.metaResourceSegments.last(), m_layout.metaResourceSegments.last());

        layout.operationsSegment = QInstaller::retrieveInt64Range(&binary).moved(layout
            .endOfExectuable);
        QCOMPARE(layout.operationsSegment, m_layout.operationsSegment);

        QCOMPARE(layout.metaSegmentsCount, QInstaller::retrieveInt64(&binary));

        layout.binaryContentSize = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.binaryContentSize, m_layout.binaryContentSize);
        QCOMPARE(layout.endOfExectuable, layout.endOfBinaryContent - layout.binaryContentSize);

        layout.magicMarker = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.magicMarker, m_layout.magicMarker);

        layout.magicCookie = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.magicCookie, m_layout.magicCookie);

        binary.seek(layout.operationsSegment.start());

        layout.operationsCount = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.operationsCount, m_layout.operationsCount);

        for (int i = 0; i < layout.operationsCount; ++i) {
            QCOMPARE(m_operations.at(i).name, QInstaller::retrieveString(&binary));
            QCOMPARE(m_operations.at(i).xml, QInstaller::retrieveString(&binary));
        }

        layout.operationsCount = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.operationsCount, m_layout.operationsCount);

        layout.collectionCount = QInstaller::retrieveInt64(&binary);
        QCOMPARE(layout.collectionCount, m_layout.collectionCount);

        binary.seek(layout.resourceCollectionsSegment.start());
        m_manager.read(&binary, layout.endOfExectuable);

        const QList<ResourceCollection> collections = m_manager.collections();
        QCOMPARE(collections.count(), m_layout.collectionCount);

        ResourceCollection collection = m_manager.collectionByName(QByteArray("Collection 1"));
        QCOMPARE(collection.resources().count(), 1);

        QSharedPointer<Resource> resource(collection.resourceByName(QByteArray("Resource 1")));
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 1, Resource 1."));
        resource->close();

        collection = m_manager.collectionByName(QByteArray("Collection 2"));
        QCOMPARE(collection.resources().count(), 1);

        resource = collection.resourceByName(QByteArray("Resource 2"));
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 2, Resource 2."));
        resource->close();
    }

    void testWriteBinaryContentFunction()
    {
        ResourceCollection collection(QByteArray("QResources"));
        foreach (const Range<qint64> &segment, m_layout.metaResourceSegments)
            collection.appendResource(QSharedPointer<Resource>(new Resource(m_binary, segment)));
        m_manager.insertCollection(collection);

        QList<OperationBlob> operations;
        foreach (const OperationBlob &operation, m_operations)
            operations.append(operation);

        QTemporaryFile file;
        QInstaller::openForWrite(&file);

        QInstaller::blockingWrite(&file, QByteArray(scTinySize, '1'));
        BinaryContent::writeBinaryContent(&file, operations, m_manager, m_layout.magicMarker,
            m_layout.magicCookie);
        file.close();

        QFile existingBinary(m_binary);
        QInstaller::openForRead(&existingBinary);

        QInstaller::openForRead(&file);
        QCOMPARE(file.readAll(), existingBinary.readAll());
    }

    void testReadBinaryContentFunction()
    {
        QFile file(m_binary);
        QInstaller::openForRead(&file);

        qint64 magicMarker;
        QList<OperationBlob> operations;
        ResourceCollectionManager manager;
        BinaryContent::readBinaryContent(&file, &operations, &manager, &magicMarker,
            m_layout.magicCookie);
        file.close();

        QCOMPARE(magicMarker, m_layout.magicMarker);

        ResourceCollection collection = manager.collectionByName("QResources");
        QCOMPARE(collection.resources().count(), m_layout.metaResourceSegments.count());
        for (int i = 0; i < collection.resources().count(); ++i)
            QCOMPARE(collection.resources().at(i)->segment(), m_layout.metaResourceSegments.at(i));

        QCOMPARE(operations.count(), m_operations.count());
        for (int i = 0; i < operations.count(); ++i) {
            QCOMPARE(operations.at(i).name, m_operations.at(i).name);
            QCOMPARE(operations.at(i).xml, m_operations.at(i).xml);
        }

        QCOMPARE(manager.collectionCount(), m_manager.collectionCount());

        collection = manager.collectionByName(QByteArray("Collection 1"));
        QCOMPARE(collection.resources().count(), 1);

        QSharedPointer<Resource> resource(collection.resourceByName(QByteArray("Resource 1")));
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 1, Resource 1."));
        resource->close();

        collection = manager.collectionByName(QByteArray("Collection 2"));
        QCOMPARE(collection.resources().count(), 1);

        resource = collection.resourceByName(QByteArray("Resource 2"));
        QCOMPARE(resource.isNull(), false);
        QCOMPARE(resource->isOpen(), false);
        QCOMPARE(resource->open(), true);
        QCOMPARE(resource->readAll(), QByteArray("Collection 2, Resource 2."));
        resource->close();
    }

    void testXmlDocumentParsing()
    {
        PackageManagerCore core;
        core.setValue(scTargetDir, QLatin1String("relocatable_targetdir"));

        TestOperation op(QLatin1String("Operation 3"), &core);
        QStringList stringListValue = (QStringList() << QLatin1String("list_value1") << QLatin1String("list_value2"));
        op.setValue(QLatin1String("string_list"), stringListValue);

        const QString stringValue = core.value(scTargetDir) + QLatin1String(", string_value1");
        op.setValue(QLatin1String("string"), stringValue);

        QVariantMap map;
        map.insert(QLatin1String("key1"), 1);
        map.insert(QLatin1String("key2"), QLatin1String("map_value2"));
        op.setValue(QLatin1String("variant_map"), map);

        QDomDocument document = op.toXml();
        QVERIFY2(document.toString().contains(QLatin1String("@RELOCATABLE_PATH@")),
                 "TargetDir not replaced with @RELOCATABLE_PATH@");

        op.fromXml(document); // Resets the operation values from written QDomDocuments

        QCOMPARE(op.value(QLatin1String("string_list")), stringListValue);
        QVERIFY2(!op.value(QLatin1String("string")).toString().contains(QLatin1String("@RELOCATABLE_PATH@")),
                 "@RELOCATABLE@ not replaced with TargetDir");
        QCOMPARE(op.value(QLatin1String("variant_map")), map);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QCOMPARE(op.value(QLatin1String("string_list")).metaType().id(), QMetaType::QStringList);
        QCOMPARE(op.value(QLatin1String("string")).metaType().id(), QMetaType::QString);
        QCOMPARE(op.value(QLatin1String("variant_map")).metaType().id(), QMetaType::QVariantMap);
#else
        QCOMPARE(op.value(QLatin1String("string_list")).type(), QMetaType::QStringList);
        QCOMPARE(op.value(QLatin1String("string")).type(), QMetaType::QString);
        QCOMPARE(op.value(QLatin1String("variant_map")).type(), QMetaType::QVariantMap);
#endif
    }

    void cleanupTestCase()
    {
        m_manager.clear();
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
