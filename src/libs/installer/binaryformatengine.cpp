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

#include "binaryformatengine.h"

#include <QRegularExpression>

namespace {

class StringListIterator : public QAbstractFileEngineIterator
{
public:
    StringListIterator( const QStringList &list, QDir::Filters filters, const QStringList &nameFilters)
        : QAbstractFileEngineIterator(filters, nameFilters)
        , list(list)
        , index(-1)
    {
    }

    bool hasNext() const
    {
        return index < list.size() - 1;
    }

    QString next()
    {
        if(!hasNext())
            return QString();
        ++index;
        return currentFilePath();
    }

    QString currentFileName() const
    {
        return index < 0 ? QString() : list[index];
    }

private:
    const QStringList list;
    int index;
};

} // anon namespace

namespace QInstaller {

/*!
    \class QInstaller::BinaryFormatEngine
    \inmodule QtInstallerFramework
    \brief The BinaryFormatEngine class is the default file engine for accessing resource
        collections and resource files.
*/

/*!
    Constructs a new binary format engine with \a collections and \a fileName.
*/
BinaryFormatEngine::BinaryFormatEngine(const QHash<QByteArray, ResourceCollection> &collections,
        const QString &fileName)
    : m_resource(nullptr)
    , m_collections(collections)
{
    setFileName(fileName);
}

/*!
    \internal

    Sets the file engine's file name to \a file. This is the file that the rest of the virtual
    functions will operate on.
*/
void BinaryFormatEngine::setFileName(const QString &file)
{
    m_fileNamePath = file;

    static const QChar sep = QLatin1Char('/');
    static const QString prefix = QLatin1String("installer://");
    Q_ASSERT(m_fileNamePath.toLower().startsWith(prefix));

    // cut the prefix
    QString path = m_fileNamePath.mid(prefix.length());
    while (path.endsWith(sep))
        path.chop(1);

    m_collection = m_collections.value(path.section(sep, 0, 0).toUtf8());
    m_collection.setName(path.section(sep, 0, 0).toUtf8());
    m_resource = m_collection.resourceByName(path.section(sep, 1, 1).toUtf8());
}

/*!
    \internal
*/
bool BinaryFormatEngine::close()
{
    if (m_resource.isNull())
        return false;

    const bool result = m_resource->isOpen();
    m_resource->close();
    return result;
}

/*!
    \internal
*/
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
bool BinaryFormatEngine::open(QIODevice::OpenMode mode)
#else
bool BinaryFormatEngine::open(QIODevice::OpenMode mode, std::optional<QFile::Permissions> permissions)
#endif
{
    Q_UNUSED(mode)
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    Q_UNUSED(permissions)
#endif
    return m_resource.isNull() ? false : m_resource->open();
}

/*!
    \internal
*/
qint64 BinaryFormatEngine::pos() const
{
    return m_resource.isNull() ? 0 : m_resource->pos();
}

/*!
    \internal
*/
qint64 BinaryFormatEngine::read(char *data, qint64 maxlen)
{
    return m_resource.isNull() ? -1 : m_resource->read(data, maxlen);
}

/*!
    \internal
*/
bool BinaryFormatEngine::seek(qint64 offset)
{
    return m_resource.isNull() ? false : m_resource->seek(offset);
}

/*!
    \internal
*/
QString BinaryFormatEngine::fileName(FileName file) const
{
    static const QChar sep = QLatin1Char('/');
    switch(file) {
        case BaseName:
            return m_fileNamePath.section(sep, -1, -1, QString::SectionSkipEmpty);
        case PathName:
        case AbsolutePathName:
        case CanonicalPathName:
            return m_fileNamePath.section(sep, 0, -2, QString::SectionSkipEmpty);
        case DefaultName:
        case AbsoluteName:
        case CanonicalName:
            return m_fileNamePath;
        default:
            return QString();
    }
}

/*!
    \internal
*/
bool BinaryFormatEngine::copy(const QString &newName)
{
    if (QFile::exists(newName))
        return false;

    QFile target(newName);
    if (!target.open(QIODevice::WriteOnly))
        return false;

    qint64 bytesLeft = size();
    if (!open(QIODevice::ReadOnly))
        return false;

    char data[4096];
    while(bytesLeft > 0) {
        const qint64 len = qMin<qint64>(bytesLeft, 4096);
        const qint64 bytesRead = read(data, len);
        if (bytesRead != len) {
            close();
            return false;
        }
        const qint64 bytesWritten = target.write(data, len);
        if (bytesWritten != len) {
            close();
            return false;
        }
        bytesLeft -= len;
    }
    close();

    return true;
}

/*!
    \internal
*/
QAbstractFileEngine::FileFlags BinaryFormatEngine::fileFlags(FileFlags type) const
{
    FileFlags result;
    if ((type & FileType) && (!m_resource.isNull()))
        result |= FileType;
    if ((type & DirectoryType) && m_resource.isNull())
        result |= DirectoryType;
    if ((type & ExistsFlag) && (!m_resource.isNull()))
        result |= ExistsFlag;
    if ((type & ExistsFlag) && m_resource.isNull() && (!m_collection.name().isEmpty()))
        result |= ExistsFlag;

    return result;
}

/*!
    \internal
*/
QAbstractFileEngineIterator *BinaryFormatEngine::beginEntryList(QDir::Filters filters, const QStringList &filterNames)
{
    const QStringList entries = entryList(filters, filterNames);
    return new StringListIterator(entries, filters, filterNames);
}

/*!
    \internal
*/
QStringList BinaryFormatEngine::entryList(QDir::Filters filters, const QStringList &filterNames) const
{
    if (!m_resource.isNull())
        return QStringList();

    QStringList result;
    if ((!m_collection.name().isEmpty()) && (filters & QDir::Files)) {
        foreach (const QSharedPointer<Resource> &resource, m_collection.resources())
            result.append(QString::fromUtf8(resource->name()));
    } else if (m_collection.name().isEmpty() && (filters & QDir::Dirs)) {
        foreach (const ResourceCollection &collection, m_collections)
            result.append(QString::fromUtf8(collection.name()));
    }
    result.removeAll(QString()); // Remove empty names, will crash while using directory iterator.

    if (filterNames.isEmpty())
        return result;

    QList<QRegularExpression> regexps;
    for (const QString &i : filterNames) {
        regexps.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(i),
                                          QRegularExpression::CaseInsensitiveOption));
    }

    QStringList entries;
    for (const QString &i : qAsConst(result)) {
        bool matched = false;
        for (const QRegularExpression &reg : qAsConst(regexps)) {
            matched = reg.match(i).hasMatch();
            if (matched)
                break;
        }
        if (matched)
            entries.append(i);
    }

    return entries;
}

/*!
    \internal
*/
qint64 BinaryFormatEngine::size() const
{
    return m_resource.isNull() ? 0 : m_resource->size();
}

} // namespace QInstaller
