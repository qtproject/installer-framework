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

#include "../shared/packagemanager.h"

#include <errors.h>
#include <fileutils.h>
#include <metadatacache.h>
#include <metadata.h>
#include <repository.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTest>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

using namespace QInstaller;

static const QByteArray scPlaceholderSha1("placeholder_sha1");

class tst_metadatacache : public QObject
{
    Q_OBJECT

private:
    void copyExistingCacheFromResourceTree()
    {
        try {
            QInstaller::copyDirectoryContents(":/data/existing-cache/", m_cachePath);

            // We need to modify the test data here because the checksums of
            // files may differ on Windows and Unix platforms.
            QVERIFY(QDir().rename(m_cachePath + QDir::separator() + scPlaceholderSha1,
                m_cachePath + QDir::separator() + m_oldMetadataItemChecksum));

            QFile manifestFile(m_cachePath + QDir::separator() + "manifest.json");
            // The file lost the write bit after copying from resource
            QInstaller::setDefaultFilePermissions(&manifestFile, QInstaller::NonExecutable);
            QVERIFY2(manifestFile.open(QIODevice::ReadWrite), qPrintable(manifestFile.errorString()));

            const QByteArray manifestData = manifestFile.readAll();
            QJsonDocument manifestJsonDoc(QJsonDocument::fromJson(manifestData));
            QJsonObject docJsonObject = manifestJsonDoc.object();

            QJsonArray itemsJsonArray;
            itemsJsonArray.append(QJsonValue(QLatin1String(m_oldMetadataItemChecksum)));

            docJsonObject.insert(QLatin1String("items"), itemsJsonArray);
            manifestJsonDoc.setObject(docJsonObject);

            manifestFile.seek(0);
            QVERIFY(manifestFile.write(manifestJsonDoc.toJson()) != -1);

        } catch (const Error &e) {
            QVERIFY2(false, QString::fromLatin1("Error while copying item to path %1: %2")
                .arg(m_cachePath, e.message()).toLatin1());
        }
    }

    QStringList itemsFromManifest(const QString &manifestPath)
    {
        QFile manifestFile(manifestPath);
        if (!manifestFile.open(QIODevice::ReadOnly))
            return QStringList();

        const QByteArray manifestData = manifestFile.readAll();
        const QJsonDocument manifestJsonDoc(QJsonDocument::fromJson(manifestData));
        const QJsonObject docJsonObject = manifestJsonDoc.object();
        const QJsonArray itemsJsonArray = docJsonObject.value(QLatin1String("items")).toArray();

        QStringList items;
        for (const auto &itemJsonValue : itemsJsonArray)
            items << itemJsonValue.toString();

        return items;
    }

    QByteArray checksumFromUpdateFile(const QString &directory)
    {
        QFile updateFile(directory + QDir::separator() + QLatin1String("Updates.xml"));
        if (!updateFile.open(QIODevice::ReadOnly))
            return QByteArray();

        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(&updateFile);
        return hash.result().toHex();
    }

private slots:
    void init()
    {
        m_cachePath = generateTemporaryFileName();
    }

    void cleanup()
    {
        if (QFileInfo::exists(m_cachePath))
            QInstaller::removeDirectory(m_cachePath, true);
    }

    void initTestCase()
    {
        m_newMetadataItemChecksum = checksumFromUpdateFile(":/data/local-temp-repository");
        m_oldMetadataItemChecksum = checksumFromUpdateFile(":/data/existing-cache/"
            + QLatin1String(scPlaceholderSha1));

        QVERIFY(!m_newMetadataItemChecksum.isEmpty());
        QVERIFY(!m_oldMetadataItemChecksum.isEmpty());

        qInstallMessageHandler(silentTestMessageHandler);
    }

    void testRegisterItemToEmptyCache()
    {
        MetadataCache cache(m_cachePath);
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");

        QVERIFY(cache.registerItem(metadata));
        metadata = cache.itemByChecksum(m_newMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());
        QVERIFY(!QFileInfo::exists(m_cachePath + "/manifest.json"));
        QVERIFY(cache.sync());
        QVERIFY(itemsFromManifest(m_cachePath + "/manifest.json").contains(QLatin1String(m_newMetadataItemChecksum)));

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testRegisterItemToExistingCache()
    {
        copyExistingCacheFromResourceTree();

        MetadataCache cache(m_cachePath);
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");
        QVERIFY(itemsFromManifest(m_cachePath + "/manifest.json").contains(QLatin1String(m_oldMetadataItemChecksum)));

        QVERIFY(cache.registerItem(metadata));
        metadata = cache.itemByChecksum(m_newMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());
        QVERIFY(cache.sync());
        const QStringList manifestItems = itemsFromManifest(m_cachePath + "/manifest.json");
        QVERIFY(manifestItems.contains(QLatin1String(m_oldMetadataItemChecksum)));
        QVERIFY(manifestItems.contains(QLatin1String(m_newMetadataItemChecksum)));

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testRegisterItemFails()
    {
        // 1. Test fail due to invalidated cache
        MetadataCache cache;
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");

        QVERIFY(!cache.registerItem(metadata));
        QCOMPARE(cache.errorString(), "Cannot register item to invalidated cache.");

        delete metadata;
        metadata = nullptr;

        // 2. Test fail due to null metadata
        cache.setPath(m_cachePath);
        cache.setType("Metadata");
        cache.setVersion(QUOTE(IFW_CACHE_FORMAT_VERSION));
        QVERIFY(cache.initialize());

        QVERIFY(!cache.registerItem(metadata));
        QCOMPARE(cache.errorString(), "Cannot register null item.");

        // 3. Test fail due to invalid metadata
        metadata = new Metadata;
        QVERIFY(!cache.registerItem(metadata));
        QCOMPARE(cache.errorString(), "Cannot register invalid item with checksum ");

        // 4. Test fail due to duplicate metadata item
        metadata->setPath(":/data/local-temp-repository/");
        QVERIFY(cache.registerItem(metadata));
        QVERIFY(cache.itemByChecksum(m_newMetadataItemChecksum)->isValid());
        QVERIFY(!cache.registerItem(metadata));
        QCOMPARE(cache.errorString(), QString::fromLatin1("Cannot register item with checksum "
            "%1. An item with the same checksum already exists in cache.")
            .arg(QString::fromLatin1(m_newMetadataItemChecksum)).toLatin1());

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testInitializeExistingCache()
    {
        copyExistingCacheFromResourceTree();

        MetadataCache cache(m_cachePath);
        Metadata *metadata = cache.itemByChecksum(m_oldMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testInitializeForeignCache_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<QString>("version");

        QTest::newRow("Type mismatch") << "MyCacheableType" << QUOTE(IFW_CACHE_FORMAT_VERSION);
        QTest::newRow("Version mismatch") << "Metadata" << "0.9.1";
    }

    void testInitializeForeignCache()
    {
        QFETCH(QString, type);
        QFETCH(QString, version);

        copyExistingCacheFromResourceTree();

        MetadataCache cache;
        cache.setType(type);
        cache.setVersion(version);
        cache.setPath(m_cachePath);
        QVERIFY(cache.initialize());

        QVERIFY(cache.isValid());
        QVERIFY(!cache.itemByChecksum(m_oldMetadataItemChecksum));

        QVERIFY(cache.clear());
        // The 'foreign' entry prevents removing the directory
        QVERIFY(QFileInfo::exists(m_cachePath));
    }

    void testInitializeCacheFails()
    {
        MetadataCache cache;
        QVERIFY(!cache.initialize());
        QCOMPARE(cache.errorString(), "Cannot initialize cache with empty path.");
    }

    void testRemoveItemFromCache()
    {
        copyExistingCacheFromResourceTree();

        MetadataCache cache(m_cachePath);
        Metadata *metadata = cache.itemByChecksum(m_oldMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());
        QVERIFY(cache.removeItem(m_oldMetadataItemChecksum));

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testRemoveItemFails()
    {
        copyExistingCacheFromResourceTree();

        MetadataCache cache(m_cachePath);
        QVERIFY(!cache.removeItem("12345"));
        QCOMPARE(cache.errorString(), "Cannot remove item specified by checksum 12345: no such item exists.");

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testRetrieveItemFromCache()
    {
        MetadataCache cache(m_cachePath);
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");

        QVERIFY(cache.registerItem(metadata));
        metadata = cache.itemByChecksum(m_newMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());

        metadata = cache.itemByPath(metadata->path());
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testRetrieveItemFails()
    {
        MetadataCache cache(m_cachePath);
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");
        const QString metadataPath = metadata->path();

        QVERIFY(cache.registerItem(metadata));
        QVERIFY(cache.clear());

        QVERIFY(!cache.itemByChecksum(m_newMetadataItemChecksum));
        QVERIFY(!cache.itemByPath(metadataPath));
        QCOMPARE(cache.errorString(), "Cannot retrieve item from invalidated cache.");

        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testItemObsoletesOther()
    {
        copyExistingCacheFromResourceTree();

        MetadataCache cache(m_cachePath);
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");

        QVERIFY(cache.registerItem(metadata));
        metadata->setRepository(Repository(QUrl("file:///example-repository"), true));
        metadata->setPersistentRepositoryPath(QUrl("file:///example-repository"));
        QVERIFY(metadata->isActive());

        metadata = cache.itemByChecksum(m_newMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());

        metadata = cache.itemByChecksum(m_oldMetadataItemChecksum);
        QVERIFY(metadata);
        QVERIFY(metadata->isValid());

        Metadata *obsolete = cache.obsoleteItems().first();
        QVERIFY(!obsolete->isActive());
        QCOMPARE(obsolete->checksum(), m_oldMetadataItemChecksum);

        QVERIFY(cache.clear());
        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

    void testClearCacheFails()
    {
        MetadataCache cache(m_cachePath);
        Metadata *metadata = new Metadata(":/data/local-temp-repository/");

        QVERIFY(cache.registerItem(metadata));
        QVERIFY(cache.clear());
        QVERIFY(!cache.clear());
        QCOMPARE(cache.errorString(), "Cannot clear invalidated cache.");

        QVERIFY(!QFileInfo::exists(m_cachePath));
    }

private:
    QString m_cachePath;
    QByteArray m_newMetadataItemChecksum;
    QByteArray m_oldMetadataItemChecksum;
};

QTEST_MAIN(tst_metadatacache)

#include "tst_metadatacache.moc"
