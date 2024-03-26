/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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
#include <QTextDocument>
#include <QMetaEnum>

#include "globals.h"

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
#include <termios.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <conio.h>
#endif
#include <iostream>

const char IFW_SERVER[] = "ifw.server";
const char IFW_INSTALLER_INSTALLLOG[] = "ifw.installer.installlog";
const char IFW_DEVELOPER_BUILD[] = "ifw.developer.build";

// Internal-only, hidden in --help text
const char IFW_PROGRESS_INDICATOR[] = "ifw.progress.indicator";

namespace QInstaller
{

/*!
    \fn QInstaller::lcServer()
    \internal
*/

/*!
    \fn QInstaller::lcInstallerInstallLog()
    \internal
*/

/*!
    \fn QInstaller::lcProgressIndicator()
    \internal
*/

/*!
    \fn QInstaller::lcDeveloperBuild()
    \internal
*/

/*!
    \fn inline QInstaller::splitStringWithComma(const QString &value())
    Splits \a value into substrings wherever comma occurs, and returns the list of those strings.
    \internal
*/

Q_LOGGING_CATEGORY(lcServer, IFW_SERVER)
Q_LOGGING_CATEGORY(lcInstallerInstallLog, IFW_INSTALLER_INSTALLLOG)
Q_LOGGING_CATEGORY(lcProgressIndicator, IFW_PROGRESS_INDICATOR)
Q_LOGGING_CATEGORY(lcDeveloperBuild, IFW_DEVELOPER_BUILD)

/*!
    Returns available logging categories.
*/
QStringList loggingCategories()
{
    static QStringList categories = QStringList()
            << QLatin1String(IFW_INSTALLER_INSTALLLOG)
            << QLatin1String(IFW_SERVER)
            << QLatin1String(IFW_DEVELOPER_BUILD)
            << QLatin1String("js");
    return categories;

}

Q_GLOBAL_STATIC_WITH_ARGS(QRegularExpression, staticCommaRegExp, (QLatin1String("(, |,)")));

/*!
    \internal
*/
QRegularExpression commaRegExp()
{
    return *staticCommaRegExp();
}

/*!
    Converts and returns a string \a html containing an HTML document as a plain text.
*/
QString htmlToString(const QString &html)
{
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText();
}

/*!
    \internal
*/
QString enumToString(const QMetaObject& metaObject, const char *enumerator, int key)
{
    QString value = QString();
    int enumIndex = metaObject.indexOfEnumerator(enumerator);
    if (enumIndex != -1) {
        QMetaEnum en = metaObject.enumerator(enumIndex);
        value = QLatin1String(en.valueToKey(key));
    }
    return value;
}

void askForCredentials(QString *username, QString *password, const QString &usernameTitle, const QString &passwordTitle)
{
    std::string usernameStdStr;
    std::string passwordStdStr;

    std::cout << qPrintable(usernameTitle);
    std::cin >> usernameStdStr;

    std::cout << qPrintable(passwordTitle);
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    termios oldTerm;
    termios term;

    // Turn off echoing
    tcgetattr(STDIN_FILENO, &oldTerm);
    term = oldTerm;
    term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    std::cin >> passwordStdStr;

    // Clear input buffer
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Restore old attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
#elif defined(Q_OS_WIN)
    char ch;
    while ((ch = _getch()) != '\r') { // Return key
        if (ch == '\b') { // Backspace key
            if (!passwordStdStr.empty())
                passwordStdStr.pop_back();
        } else {
            passwordStdStr.push_back(ch);
        }
    }
    // Clear input buffer
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

#endif
    std::cout << "\n";

    *username = username->fromStdString(usernameStdStr);
    *password = password->fromStdString(passwordStdStr);
}

} // namespace QInstaller

