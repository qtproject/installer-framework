/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include "globals.h"

const char IFW_COMPONENT_CHECKER[] = "ifw.componentChecker";
const char IFW_RESOURCES[] = "ifw.resources";
const char IFW_TRANSLATIONS[] = "ifw.translations";
const char IFW_NETWORK[] = "ifw.network";

namespace QInstaller
{

Q_LOGGING_CATEGORY(lcComponentChecker, IFW_COMPONENT_CHECKER)
Q_LOGGING_CATEGORY(lcResources, IFW_RESOURCES)
Q_LOGGING_CATEGORY(lcTranslations, IFW_TRANSLATIONS)
Q_LOGGING_CATEGORY(lcNetwork, IFW_NETWORK)

QStringList loggingCategories()
{
    static QStringList categories = QStringList()
            << QLatin1String(IFW_COMPONENT_CHECKER)
            << QLatin1String(IFW_RESOURCES)
            << QLatin1String(IFW_TRANSLATIONS)
            << QLatin1String(IFW_NETWORK);
    return categories;
}

Q_GLOBAL_STATIC_WITH_ARGS(QRegExp, staticCommaRegExp, (QLatin1String("(, |,)")));
QRegExp commaRegExp()
{
    return *staticCommaRegExp();
}

} // namespace QInstaller

