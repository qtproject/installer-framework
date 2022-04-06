/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "archivefactory.h"
#ifdef IFW_LIBARCHIVE
#include "libarchivewrapper.h"
#elif defined(IFW_LIB7Z)
#include "lib7zarchive.h"
#endif

#include <QFileInfo>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ArchiveFactory
    \brief The ArchiveFactory class is used to create archive objects
           based on the suffix of a given filename.

    This class acts as a factory for \c QInstaller::AbstractArchive. You can
    register one or more archive handlers with this factory and create
    registered objects based on the file suffix.

    This class follows the singleton design pattern. Only one instance
    of this class can be created and its reference can be fetched from
    the \c {instance()} method.

    Depending of the configuration features set at build time, one of the
    following archive handlers is registered by default:
    \list
        \li Lib7z
        \li LibArchive
    \endlist
*/

/*!
    \fn void QInstaller::ArchiveFactory::registerArchive(const QString &name, const QStringList &types)

    Registers a new archive handler with the factory based on \a name and list
    of supported file suffix \a types.
*/


/*!
    Returns the only instance of this class.
*/
ArchiveFactory &ArchiveFactory::instance()
{
    static ArchiveFactory instance;
    return instance;
}

/*!
    Constructs and returns a pointer to an archive object with \a filename and \a parent.
    If the archive type referenced by \a filename is not registered, a null pointer is
    returned instead.
*/
AbstractArchive *ArchiveFactory::create(const QString &filename, QObject *parent) const
{
    const QString suffix = QFileInfo(filename).completeSuffix();
    QString name;
    for (auto &types : m_supportedTypesHash) {
        QStringList::const_iterator it;
        for (it = types.constBegin(); it != types.constEnd(); ++it) {
            if (suffix.endsWith(*it, Qt::CaseInsensitive)) {
                name = m_supportedTypesHash.key(types);
                break;
            }
        }
    }
    if (name.isEmpty())
        return nullptr;

    AbstractArchive *archive = GenericFactory<AbstractArchive, QString, QString, QObject *>
        ::create(name, filename, parent);

    return archive;
}

/*!
    Returns a list of supported archive types.
*/
QStringList ArchiveFactory::supportedTypes()
{
    QStringList types;
    QHash<QString, QStringList> *const typesHash = &instance().m_supportedTypesHash;
    for (auto &value : *typesHash)
        types.append(value);

    return types;
}

/*!
    Returns \c true if the archive type from \a filename is registered with
    an archive handler.
*/
bool ArchiveFactory::isSupportedType(const QString &filename)
{
    const QString suffix = QFileInfo(filename).completeSuffix();
    QHash<QString, QStringList> *const typesHash = &instance().m_supportedTypesHash;
    for (auto &types : *typesHash) {
        QStringList::const_iterator it;
        for (it = types.constBegin(); it != types.constEnd(); ++it) {
            if (suffix.endsWith(*it, Qt::CaseInsensitive))
                return true;
        }
    }
    return false;
}

/*!
    Private constructor for ArchiveFactory. Registers default archive handlers.
*/
ArchiveFactory::ArchiveFactory()
{
#ifdef IFW_LIBARCHIVE
    registerArchive<LibArchiveWrapper>(QLatin1String("LibArchive"), QStringList()
        << QLatin1String("tar") << QLatin1String("tar.gz") << QLatin1String("tar.bz2")
        << QLatin1String("tar.xz") << QLatin1String("zip") << QLatin1String("7z")
        << QLatin1String("qbsp"));
#elif defined(IFW_LIB7Z)
    registerArchive<Lib7zArchive>(QLatin1String("Lib7z"), QStringList()
        << QLatin1String("7z") << QLatin1String("qbsp"));
#endif
}
