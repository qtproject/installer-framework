/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "repository.h"

namespace QInstaller {

/*
    Constructs an invalid Repository object.
*/
Repository::Repository()
{
}

/*!
    Constructs a new repository by setting it's address to \a url.
*/
Repository::Repository(const QUrl &url)
    : m_url(url)
{
}

/*!
    Returns true if the repository URL is valid; otherwise returns false.

    Note: The URL is simpley run through a conformance test. It is not checked that the repository
    actually exists.
*/
bool Repository::isValid() const
{
    return m_url.isValid();
}

/*!
    Returns the URL of the repository. By default an invalid \sa QUrl is returned.
*/
QUrl Repository::url() const
{
    return m_url;
}

/*!
    Sets the repository URL to the one specified at \a url.
*/
void Repository::setUrl(const QUrl& url)
{
    m_url = url;
}

}
