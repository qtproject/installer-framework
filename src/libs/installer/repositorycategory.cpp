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

#include "repositorycategory.h"
#include "filedownloaderfactory.h"
#include "constants.h"

#include <QDataStream>
#include <QFileInfo>
#include <QStringList>

/*!
    \fn inline hashValue QInstaller::qHash(const RepositoryCategory &repository)

    Returns a hash of the repository category \a repository.
*/

namespace QInstaller {


template <typename T>
static QSet<T> variantListToSet(const QVariantList &list)
{
    QSet<T> set;
    foreach (const QVariant &variant, list)
        set.insert(variant.value<T>());
    return set;
}

/*!
    Constructs an uninitialized RepositoryCategory object.
*/
RepositoryCategory::RepositoryCategory()
    : m_enabled(false)
{
    registerMetaType();
}

/*!
    Constructs a new category by using all fields of the given category \a other.
*/
RepositoryCategory::RepositoryCategory(const RepositoryCategory &other)
    : m_data(other.m_data), m_displayname(other.m_displayname), m_tooltip(other.m_tooltip),
    m_enabled(other.m_enabled)
{
    registerMetaType();
}


void RepositoryCategory::registerMetaType()
{
    qRegisterMetaType<RepositoryCategory>("RepositoryCategory");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qRegisterMetaTypeStreamOperators<RepositoryCategory>("RepositoryCategory");
#endif
}

/*!
    Returns the Name for the category to be displayed.
*/
QString RepositoryCategory::displayname() const
{
    return m_displayname;
}

/*!
    Sets the DisplayName of the category to \a displayname.
*/
void RepositoryCategory::setDisplayName(const QString &displayname)
{
    m_displayname = displayname;
}

/*!
    Returns the Tooltip for the category to be displayed.
*/
QString RepositoryCategory::tooltip() const
{
    return m_tooltip;
}

/*!
    Sets the Tooltip of the category to \a tooltip.
*/
void RepositoryCategory::setTooltip(const QString &tooltip)
{
    m_tooltip = tooltip;
}

/*!
    Returns the list of repositories the category has.
*/
QSet<Repository> RepositoryCategory::repositories() const
{
    return variantListToSet<Repository>(m_data.values(scRepositories));
}

/*!
    Inserts a set of \a repositories to the category. Removes old \a repositories
    if \a replace is set to \c true.
*/
void RepositoryCategory::setRepositories(const QSet<Repository> repositories, const bool replace)
{
    if (replace)
        m_data.remove(scRepositories);

    foreach (const Repository &repository, repositories)
        m_data.insert(scRepositories, QVariant().fromValue(repository));
}

/*!
    Inserts \a repository to the category.
*/
void RepositoryCategory::addRepository(const Repository &repository)
{
    m_data.insert(scRepositories, QVariant().fromValue(repository));
}

/*!
    Returns whether this category is enabled and used during information retrieval.
*/
bool RepositoryCategory::isEnabled() const
{
    return m_enabled;
}

/*!
    Sets this category to \a enabled state and and thus determines whether to use this
    repository for information retrieval.

*/
void RepositoryCategory::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

/*!
    Compares the values of this category to \a other and returns true if they are equal.
*/
bool RepositoryCategory::operator==(const RepositoryCategory &other) const
{
    return m_displayname == other.m_displayname;
}

/*!
    Returns true if the \a other category is not equal to this repository; otherwise returns false. Two
    categories are considered equal if they contain the same displayname. \sa operator==()
*/
bool RepositoryCategory::operator!=(const RepositoryCategory &other) const
{
    return !(*this == other);
}

/*!
    \internal
*/
QDataStream &operator>>(QDataStream &istream, RepositoryCategory &repository)
{
    return istream;
}

/*!
    \internal
*/
QDataStream &operator<<(QDataStream &ostream, const RepositoryCategory &repository)
{
    return ostream << repository.m_displayname.toUtf8().toBase64() << repository.m_data;
}

}
