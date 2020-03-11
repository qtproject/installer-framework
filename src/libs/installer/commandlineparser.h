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

#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include "commandlineparser_p.h"

#include <QCommandLineParser>

class CommandLineParser
{
public:
    CommandLineParser();
    ~CommandLineParser();

    QString helpText() const { return m_parser.helpText(); }
    bool isSet(const QString &option) { return m_parser.isSet(option); }
    QStringList unknownOptionNames() const { return m_parser.unknownOptionNames(); }
    QStringList positionalArguments() const { return m_parser.positionalArguments(); }
    bool parse(const QStringList &argumens) { return m_parser.parse(argumens); }
    QString value(const QString &option) const { return m_parser.value(option); }

private:
    QCommandLineParser m_parser;
    class CommandLineParserPrivate *const d;
};

#endif // COMMANDLINEPARSER_H
