/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "messageboxhandler.h"

#include <QtCore/QDebug>

#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>

/*!
    \qmltype QMessageBox
    \inqmlmodule scripting

    \brief The QMessageBox type provides a modal dialog for informing the
    user or asking the user a question and receiving an answer.


    \code
    var result = QMessageBox.question("quit.question", "Installer", "Do you want to quit the installer?",
                                      QMessageBox.Yes | QMessageBox.No);
    if (result == QMessageBox.Yes) {
       // ...
    }
    \endcode

    \section2 Buttons

    QMessageBox defines a list of common buttons:
    \list
    \li QMessageBox.Ok
    \li QMessageBox.Open
    \li QMessageBox.Save
    \li QMessageBox.Cancel
    \li QMessageBox.Close
    \li QMessageBox.Discard
    \li QMessageBox.Apply
    \li QMessageBox.Reset
    \li QMessageBox.RestoreDefaults
    \li QMessageBox.Help
    \li QMessageBox.SaveAll
    \li QMessageBox.Yes
    \li QMessageBox.YesToAll
    \li QMessageBox.No
    \li QMessageBox.NoToAll
    \li QMessageBox.Abort
    \li QMessageBox.Retry
    \li QMessageBox.Ignore
    \li QMessageBox.NoButton
    \endlist

    \section2 Scripted Installations

    Sometimes it is useful to automatically close message boxes, for example during a scripted
    installation. This can be achieved by calling
    Installer::setMessageBoxAutomaticAnswer, Installer::autoAcceptMessageBoxes,
    Installer::autoRejectMessageBoxes. The \c identifier argument in the method calls
    allows to identify specific message boxes for this purpose.
 */


/*!
    \qmlmethod Button QMessageBox::critical(string identifier, string title, string text,
    Buttons buttons = QMessageBox.Ok, Button button
    = QMessageBox.NoButton)

    Opens a critical message box with the given \a title and \a text.
*/

/*!
    \qmlmethod Button QMessageBox::information(string identifier, string title, string text,
    Buttons buttons = QMessageBox.Ok, Button button
    = QMessageBox.NoButton)

    Opens an information message box with the given \a title and \a text.
*/

/*!
    \qmlmethod Button QMessageBox::question(string identifier, string title, string text,
    Buttons buttons = QMessageBox.Yes | QMessageBox.No, Button button
    = QMessageBox.NoButton)

    Opens a question message box with the given \a title and \a text.
*/

/*!
    \qmlmethod Button QMessageBox::warning(string identifier, string title, string text,
    Buttons buttons = QMessageBox.Ok, Button button
    = QMessageBox.NoButton)

    Opens a warning message box with the given \a title and \a text.
*/

using namespace QInstaller;

// -- MessageBoxHandler

MessageBoxHandler *MessageBoxHandler::m_instance = 0;

MessageBoxHandler::MessageBoxHandler(QObject *parent)
    : QObject(parent)
    , m_defaultAction(MessageBoxHandler::AskUser)
{
}

MessageBoxHandler *MessageBoxHandler::instance()
{
    if (m_instance == 0)
        m_instance = new MessageBoxHandler(qApp);
    return m_instance;
}

QWidget *MessageBoxHandler::currentBestSuitParent()
{
    if (qobject_cast<QApplication*> (qApp) == 0)
        return 0;

    if (qApp->activeModalWidget())
        return qApp->activeModalWidget();

    return qApp->activeWindow();
}

QList<QMessageBox::Button> MessageBoxHandler::orderedButtons()
{
    static QList<QMessageBox::Button> buttons;
    if (!buttons.isEmpty())
        return buttons;
    buttons << QMessageBox::YesToAll << QMessageBox::Yes << QMessageBox::Ok << QMessageBox::Apply
        << QMessageBox::SaveAll << QMessageBox::Save <<QMessageBox::Retry << QMessageBox::Ignore
        << QMessageBox::Help << QMessageBox::RestoreDefaults << QMessageBox::Reset << QMessageBox::Open
        << QMessageBox::Cancel << QMessageBox::Close << QMessageBox::Abort << QMessageBox::Discard
        << QMessageBox::No << QMessageBox::NoToAll;
    return buttons;
}

void MessageBoxHandler::setDefaultAction(DefaultAction defaultAction)
{
    if (m_defaultAction == defaultAction)
        return;
    m_defaultAction = defaultAction;

    m_buttonOrder.clear();
    if (m_defaultAction != AskUser) {
        m_buttonOrder = orderedButtons();
    }

    if (m_defaultAction == Reject) {
        // If we want to reject everything, we need the lowest button. For example, if Cancel is existing it
        // could use Cancel, but if Close is existing it will use Close.
        std::reverse(m_buttonOrder.begin(), m_buttonOrder.end());
    }
}

void MessageBoxHandler::setAutomaticAnswer(const QString &identifier, QMessageBox::StandardButton answer)
{
    m_automaticAnswers.insert(identifier, answer);
}

// -- static

QMessageBox::StandardButton MessageBoxHandler::critical(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(criticalType, parent, identifier, title, text, buttons, button);
}

QMessageBox::StandardButton MessageBoxHandler::information(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(informationType, parent, identifier, title, text, buttons, button);
}

QMessageBox::StandardButton MessageBoxHandler::question(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(questionType, parent, identifier, title, text, buttons, button);
}

QMessageBox::StandardButton MessageBoxHandler::warning(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(warningType, parent, identifier, title, text, buttons, button);
}

// -- invokable

int MessageBoxHandler::critical(const QString &identifier, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons, QMessageBox::StandardButton button) const
{
    return showMessageBox(criticalType, currentBestSuitParent(), identifier, title, text, buttons, button);
}

int MessageBoxHandler::information(const QString &identifier, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons, QMessageBox::StandardButton button) const
{
    return showMessageBox(informationType, currentBestSuitParent(), identifier, title, text, buttons, button);
}

int MessageBoxHandler::question(const QString &identifier, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons, QMessageBox::StandardButton button) const
{
    return showMessageBox(questionType, currentBestSuitParent(), identifier, title, text, buttons, button);
}

int MessageBoxHandler::warning(const QString &identifier, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons, QMessageBox::StandardButton button) const
{
    return showMessageBox(warningType, currentBestSuitParent(), identifier, title, text, buttons, button);
}

// -- private

QMessageBox::StandardButton MessageBoxHandler::autoReply(QMessageBox::StandardButtons buttons) const
{
    if (buttons == QMessageBox::NoButton)
        return QMessageBox::NoButton;

    foreach (const QMessageBox::StandardButton &currentButton, m_buttonOrder) {
        if ((buttons & currentButton) != 0)
            return currentButton;
    }
    Q_ASSERT(!"the list must have all possible buttons");
    return QMessageBox::NoButton;
}

static QMessageBox::StandardButton showNewMessageBox(QWidget *parent, QMessageBox::Icon icon,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    QMessageBox msgBox(icon, title, text, QMessageBox::NoButton, parent);
    QDialogButtonBox *buttonBox = msgBox.findChild<QDialogButtonBox *>();
    Q_ASSERT(buttonBox != 0);

    uint mask = QMessageBox::FirstButton;
    while (mask <= QMessageBox::LastButton) {
        uint sb = buttons & mask;
        mask <<= 1;
        if (!sb)
            continue;
        QPushButton *button = msgBox.addButton((QMessageBox::StandardButton)sb);
        // Choose the first accept role as the default
        if (msgBox.defaultButton())
            continue;
        if ((defaultButton == QMessageBox::NoButton
            && buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
            || (defaultButton != QMessageBox::NoButton && sb == uint(defaultButton))) {
                msgBox.setDefaultButton(button);
        }
    }
#if defined(Q_OS_MAC)
    msgBox.setWindowModality(Qt::WindowModal);
#endif
    if (msgBox.exec() == -1)
        return QMessageBox::Cancel;
    return msgBox.standardButton(msgBox.clickedButton());
}

QMessageBox::StandardButton MessageBoxHandler::showMessageBox(MessageType messageType, QWidget *parent,
    const QString &identifier, const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton) const
{
    static QHash<MessageType, QString> messageTypeHash;
    if (messageTypeHash.isEmpty()) {
        messageTypeHash.insert(criticalType, QLatin1String("critical"));
        messageTypeHash.insert(informationType, QLatin1String("information"));
        messageTypeHash.insert(questionType, QLatin1String("question"));
        messageTypeHash.insert(warningType, QLatin1String("warning"));
    };

    qDebug() << QString::fromLatin1("created %1 message box %2: '%3', %4").arg(messageTypeHash
        .value(messageType),identifier, title, text);

    if (qobject_cast<QApplication*> (qApp) == 0)
        return defaultButton;

    if (m_automaticAnswers.contains(identifier))
        return m_automaticAnswers.value(identifier);

    if (m_defaultAction == AskUser) {
        switch (messageType) {
            case criticalType:
                return showNewMessageBox(parent, QMessageBox::Critical, title, text, buttons, defaultButton);
            case informationType:
                return showNewMessageBox(parent, QMessageBox::Information, title, text, buttons, defaultButton);
            case questionType:
                return showNewMessageBox(parent, QMessageBox::Question, title, text, buttons, defaultButton);
            case warningType:
                return showNewMessageBox(parent, QMessageBox::Warning, title, text, buttons, defaultButton);
        }
    } else {
        return autoReply(buttons);
    }

    Q_ASSERT_X(false, Q_FUNC_INFO, "Something went really wrong.");
    return defaultButton;
}
