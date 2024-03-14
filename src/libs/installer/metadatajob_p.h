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

#ifndef METADATAJOB_P_H
#define METADATAJOB_P_H

#include "archivefactory.h"
#include "metadatajob.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

namespace QInstaller{

class UnzipArchiveException : public QException
{
public:
    UnzipArchiveException() {}
    ~UnzipArchiveException() throw() {}
    explicit UnzipArchiveException(const QString &message)
        : m_message(message)
    {}

    void raise() const override { throw *this; }
    QString message() const { return m_message; }
    UnzipArchiveException *clone() const override{ return new UnzipArchiveException(*this); }

private:
    QString m_message;
};

class UnzipArchiveTask : public AbstractTask<void>
{
    Q_OBJECT
    Q_DISABLE_COPY(UnzipArchiveTask)

public:
    UnzipArchiveTask(const QString &arcive, const QString &target)
        : m_archive(arcive)
        , m_targetDir(target)
        , m_removeArchive(false)
        , m_storeChecksums(false)
    {}

    QString target() { return m_targetDir; }
    QString archive() { return m_archive; }
    void setRemoveArchive(bool remove) { m_removeArchive = remove; }
    void setStoreChecksums(bool store) { m_storeChecksums = store; }

    void doTask(QFutureInterface<void> &fi) override
    {
        fi.reportStarted();
        fi.setExpectedResultCount(1);

        if (fi.isCanceled()) {
            fi.reportFinished();
            return; // ignore already canceled
        }

        QScopedPointer<AbstractArchive> archive(ArchiveFactory::instance().create(m_archive));
        if (!archive) {
            fi.reportException(UnzipArchiveException(MetadataJob::tr("Unsupported archive \"%1\": no handler "
                "registered for file suffix \"%2\".").arg(m_archive, QFileInfo(m_archive).suffix())));
            return;
        } else if (!archive->open(QIODevice::ReadOnly)) {
            fi.reportException(UnzipArchiveException(MetadataJob::tr("Cannot open file \"%1\" for "
                "reading: %2").arg(QDir::toNativeSeparators(m_archive), archive->errorString())));
            return;
        } else if (!archive->extract(m_targetDir)) {
            fi.reportException(UnzipArchiveException(MetadataJob::tr("Error while extracting "
                "archive \"%1\": %2").arg(QDir::toNativeSeparators(m_archive), archive->errorString())));
            return;
        }

        if (m_storeChecksums) {
            // Calculate and store checksums of extracted files for later use
            const QVector<ArchiveEntry> entries = archive->list();
            for (auto &entry : entries) {
                if (entry.isDirectory)
                    continue;

                QFile file(m_targetDir + QDir::separator() + entry.path);
                if (!file.open(QIODevice::ReadOnly)) {
                    fi.reportException(UnzipArchiveException(MetadataJob::tr("Cannot open extracted file \"%1\" for "
                        "reading: %2").arg(QDir::toNativeSeparators(file.fileName()), file.errorString())));
                    break;
                }
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&file);

                const QByteArray hexChecksum = hash.result().toHex();
                QFileInfo fileInfo(file.fileName());
                QFile hashFile(fileInfo.absolutePath() + QDir::separator()
                    + QString::fromLatin1(hexChecksum) + QLatin1String(".sha1"));
                if (!hashFile.open(QIODevice::WriteOnly)) {
                    fi.reportException(UnzipArchiveException(MetadataJob::tr("Cannot open file \"%1\" for "
                        "writing: %2").arg(QDir::toNativeSeparators(hashFile.fileName()), hashFile.errorString())));
                    break;
                }
            }
        }

        archive->close();
        if (m_removeArchive)
            QFile::remove(m_archive);

        fi.reportFinished();
    }

private:
    QString m_archive;
    QString m_targetDir;
    bool m_removeArchive;
    bool m_storeChecksums;
};

class CacheTaskException : public QException
{
public:
    CacheTaskException() {}
    explicit CacheTaskException(const QString &message)
        : m_message(message)
    {}
    ~CacheTaskException() {}

    void raise() const override { throw *this; }
    QString message() const { return m_message; }
    CacheTaskException *clone() const override { return new CacheTaskException(*this); }

private:
    QString m_message;
};

class UpdateCacheTask : public AbstractTask<void>
{
    Q_OBJECT
    Q_DISABLE_COPY(UpdateCacheTask)

public:
    UpdateCacheTask(MetadataCache &cache, QHash<QString, Metadata *> &updates)
        : m_cache(&cache)
        , m_updates(&updates)
    {}

    void doTask(QFutureInterface<void> &fi) override
    {
        fi.reportStarted();
        fi.setExpectedResultCount(1);

        // Register items from current run to cache
        QStringList registeredKeys;
        bool success = true;
        for (auto *meta : qAsConst(*m_updates)) {
            if (!m_cache->registerItem(meta, true, MetadataCache::Move)) {
                success = false;
                break;
            }
            meta->setPersistentRepositoryPath(meta->repository().url());
            registeredKeys.append(m_updates->key(meta));
        }
        // Remove items whose ownership was transferred to cache
        for (auto &key : qAsConst(registeredKeys))
            m_updates->remove(key);

        // Bail out if there was error while registering items
        if (!success) {
            fi.reportException(CacheTaskException(m_cache->errorString() + u' '
                + MetadataJob::tr("Clearing the cache directory and restarting the application may solve this.")));
            m_cache->sync();
            fi.reportFinished();
            return;
        }

        // ...and clean up obsolete cached items
        const QList<Metadata *> obsolete = m_cache->obsoleteItems();
        for (auto *meta : obsolete)
            m_cache->removeItem(meta->checksum());

        if (!m_cache->sync()) {
            fi.reportException(CacheTaskException(m_cache->errorString() + u' '
                + MetadataJob::tr("Clearing the cache directory and restarting the application may solve this.")));
        }

        fi.reportFinished();
    }

private:
    MetadataCache *const m_cache;
    QHash<QString, Metadata *> *const m_updates;
};

}   // namespace QInstaller

#endif  // METADATAJOB_P_H
