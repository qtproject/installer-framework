/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdupdaterupdatesourcesinfo.h"

namespace KDUpdater {

/*!
    \inmodule kdupdater
    \class KDUpdater::UpdateSourcesInfo
    \brief The UpdateSourcesInfo class provides access to information about the update sources set
        for the application.

    An update source is a repository that contains updates applicable for the application.
    Applications can download updates from the update source and install them locally.

    Each application can have one or more update sources from which it can download updates.

    The class makes the information available through an easy to use API. You can:

    \list
        \li Get update sources information via the updateSourceInfoCount() and updateSourceInfo()
            methods.
        \li Add or remove update source information via the addUpdateSourceInfo() and
            removeUpdateSourceInfo() methods.
    \endlist
*/

/*!
    \fn KDUpdater::operator==(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs)

    Returns \c true if \a lhs and \a rhs are equal; otherwise returns \c false.
*/

/*!
    \fn KDUpdater::operator!=(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs)

    Returns \c true if \a lhs and \a rhs are different; otherwise returns \c false.
*/

struct PriorityHigherThan
{
    bool operator()(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs) const
    {
        return lhs.priority > rhs.priority;
    }
};

/*!
   Returns the number of update source info structures contained in this class.
*/
int UpdateSourcesInfo::updateSourceInfoCount() const
{
    return m_updateSourceInfoList.count();
}

/*!
   Returns the update source info structure at \a index. If an invalid index is passed, the
   function returns a \l{default-constructed value}.
*/
UpdateSourceInfo UpdateSourcesInfo::updateSourceInfo(int index) const
{
    return m_updateSourceInfoList.value(index);
}

/*!
   Adds the given update source info \a info to this class.
*/
void UpdateSourcesInfo::addUpdateSourceInfo(const UpdateSourceInfo &info)
{
    if (m_updateSourceInfoList.contains(info))
        return;
    m_updateSourceInfoList.append(info);
    std::sort(m_updateSourceInfoList.begin(), m_updateSourceInfoList.end(), PriorityHigherThan());
}

/*!
    \overload

    Adds a new update source with \a name, \a title, \a description, \a url, and \a priority to
    this class.
*/
void UpdateSourcesInfo::addUpdateSource(const QString &name, const QString &title,
    const QString &description, const QUrl &url, int priority)
{
    UpdateSourceInfo info;
    info.name = name;
    info.title = title;
    info.description = description;
    info.url = url;
    info.priority = priority;
    addUpdateSourceInfo(info);
}

/*!
   Removes the given update source info \a info from this class.
*/
void UpdateSourcesInfo::removeUpdateSourceInfo(const UpdateSourceInfo &info)
{
    m_updateSourceInfoList.removeAll(info);
}

/*!
    Clears the update source information this class holds.
*/
void UpdateSourcesInfo::clear()
{
    m_updateSourceInfoList.clear();
}

/*!
    \inmodule kdupdater
    \class KDUpdater::UpdateSourceInfo
    \brief The UpdateSourceInfo class specifies a single update source.

    An update source is a repository that contains updates applicable for the application.
    This structure describes a single update source in terms of name, title, description,
    url, and priority.
*/

/*!
    \fn UpdateSourceInfo::UpdateSourceInfo()

    Constructs an empty update source info object. The object's priority is set to -1. All other
    class members are initialized using a \l{default-constructed value}.
*/

/*!
    \variable UpdateSourceInfo::name
    \brief The name of the update source.
*/

/*!
    \variable UpdateSourceInfo::title
    \brief The title of the update source.
*/

/*!
    \variable UpdateSourceInfo::description
    \brief The description of the update source.
*/

/*!
    \variable UpdateSourceInfo::url
    \brief The URL of the update source.
*/

/*!
    \variable UpdateSourceInfo::priority
    \brief The priority of the update source.
*/

bool operator== (const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs)
{
    return lhs.name == rhs.name && lhs.title == rhs.title && lhs.description == rhs.description
        && lhs.url == rhs.url;
}

} // namespace KDUpdater
