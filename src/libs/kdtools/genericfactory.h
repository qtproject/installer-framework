/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef GENERICFACTORY_H
#define GENERICFACTORY_H

#include "kdtoolsglobal.h"

#include <QtCore/QHash>

template <typename BASE, typename IDENTIFIER = QString, typename... ARGUMENTS>
class GenericFactory
{
public:
    virtual ~GenericFactory() {}

    typedef BASE *(*FactoryFunction)(ARGUMENTS...);

    template <typename DERIVED>
    void registerProduct(const IDENTIFIER &id)
    {
        m_hash.insert(id, &GenericFactory::create<DERIVED>);
    }

    void registerProduct(const IDENTIFIER &id, FactoryFunction func)
    {
        m_hash.insert(id, func);
    }

    bool containsProduct(const IDENTIFIER &id) const
    {
        return m_hash.contains(id);
    }

    BASE *create(const IDENTIFIER &id, ARGUMENTS... args) const
    {
        const auto it = m_hash.constFind(id);
        if (it == m_hash.constEnd())
            return 0;
        return (*it)(std::forward<ARGUMENTS>(args)...);
    }

protected:
    GenericFactory() = default;

private:
    template <typename DERIVED>
    static BASE *create(ARGUMENTS... args)
    {
        return new DERIVED(std::forward<ARGUMENTS>(args)...);
    }

    GenericFactory(const GenericFactory &) = delete;
    GenericFactory &operator=(const GenericFactory &) = delete;

private:
    QHash<IDENTIFIER, FactoryFunction> m_hash;
};

#endif // GENERICFACTORY_H
