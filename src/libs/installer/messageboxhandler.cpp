/**************************************************************************
**
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
**************************************************************************/

#include "messageboxhandler.h"

#include <QtCore/QDebug>

#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::MessageBoxHandler
    \brief The MessageBoxHandler class provides a modal dialog for informing the user or asking the
    user a question and receiving an answer.


    \badcode
    var result = QMessageBox.question("quit.question", "Installer", "Do you want to quit the installer?",
                                      QMessageBox.Yes | QMessageBox.No);
    if (result == QMessageBox.Yes) {
       // ...
    }
    \endcode

    \section2 Buttons in Message Boxes

    \c QMessageBox defines a list of common buttons:
    \list
        \li \l{QMessageBox::Ok}{OK}
        \li \l{QMessageBox::Open}{Open}
        \li \l{QMessageBox::Save}{Save}
        \li \l{QMessageBox::Cancel}{Cancel}
        \li \l{QMessageBox::Close}{Close}
        \li \l{QMessageBox::Discard}{Discard}
        \li \l{QMessageBox::Apply}{Apply}
        \li \l{QMessageBox::Reset}{Reset}
        \li \l{QMessageBox::RestoreDefaults}{RestoreDefaults}
        \li \l{QMessageBox::Help}{Help}
        \li \l{QMessageBox::SaveAll}{SaveAll}
        \li \l{QMessageBox::Yes}{Yes}
        \li \l{QMessageBox::YesToAll}{YesToAll}
        \li \l{QMessageBox::No}{No}
        \li \l{QMessageBox::NoToAll}{NoToAll}
        \li \l{QMessageBox::Abort}{Abort}
        \li \l{QMessageBox::Retry}{Retry}
        \li \l{QMessageBox::Ignore}{Ignore}
        \li \l{QMessageBox::NoButton}{NoButton}
    \endlist
*/

using namespace QInstaller;

// -- MessageBoxHandler

/*!
    \enum MessageBoxHandler::DefaultAction

    This enum value holds the default action for the message box handler:

    \value  AskUser
            Ask the end user for confirmation.
    \value  Accept
            Accept the message box.
    \value  Reject
            Reject the message box.
*/

/*!
    \enum MessageBoxHandler::MessageType

    This enum value holds the severity level of the message displayed in the message box:

    \value  criticalType
            Reports critical errors.
    \value  informationType
            Reports information about normal operations.
    \value  questionType
            Asks a question during normal operations.
    \value  warningType
            Reports non-critical errors.
*/

MessageBoxHandler *MessageBoxHandler::m_instance = 0;

MessageBoxHandler::MessageBoxHandler(QObject *parent)
    : QObject(parent)
    , m_defaultAction(MessageBoxHandler::AskUser)
{
}

/*!
    Returns a message box handler instance.
*/
MessageBoxHandler *MessageBoxHandler::instance()
{
    if (m_instance == 0)
        m_instance = new MessageBoxHandler(qApp);
    return m_instance;
}

/*!
    Returns the widget or window that is most suitable to become the parent of the message box.
    Returns \c 0 if an application cannot be found.
*/
QWidget *MessageBoxHandler::currentBestSuitParent()
{
    if (qobject_cast<QApplication*> (qApp) == 0)
        return 0;

    if (qApp->activeModalWidget())
        return qApp->activeModalWidget();

    return qApp->activeWindow();
}

/*!
    Returns an ordered list of buttons to display in the message box.
*/
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

/*!
    Sets the default action for the message box to \a defaultAction.
*/
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

/*!
    Sets the button that is used as the default button and that is automatically invoked if a
    message box is shown in automatic mode. The string \a identifier is used to identify the
    message box that the default button \a answer is associated to.
*/
void MessageBoxHandler::setAutomaticAnswer(const QString &identifier, QMessageBox::StandardButton answer)
{
    m_automaticAnswers.insert(identifier, answer);
}

// -- static

/*!
    Opens a critical message box with the parent \a parent, identifier \a identifier,
    title \a title, and text \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
QMessageBox::StandardButton MessageBoxHandler::critical(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(criticalType, parent, identifier, title, text, buttons, button);
}

/*!
    Opens an information message box with the parent \a parent, identifier \a identifier,
    title \a title, and text \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
QMessageBox::StandardButton MessageBoxHandler::information(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(informationType, parent, identifier, title, text, buttons, button);
}

/*!
    Opens a question message box with the parent \a parent, identifier \a identifier,
    title \a title, and text \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
QMessageBox::StandardButton MessageBoxHandler::question(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(questionType, parent, identifier, title, text, buttons, button);
}

/*!
    Opens a warning message box with the parent \a parent, identifier \a identifier,
    title \a title, and text \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
QMessageBox::StandardButton MessageBoxHandler::warning(QWidget *parent, const QString &identifier,
    const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton button)
{
    return instance()->showMessageBox(warningType, parent, identifier, title, text, buttons, button);
}

// -- invokable

/*!
    Opens a critical message box with the identifier \a identifier, title \a title, and text
    \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
int MessageBoxHandler::critical(const QString &identifier, const QString &title,
    const QString &text, int buttons, int button) const
{
    return showMessageBox(criticalType, currentBestSuitParent(), identifier, title, text,
        QMessageBox::StandardButtons(buttons), QMessageBox::StandardButton(button));
}

/*!
    Opens an information message box with the identifier \a identifier, title \a title, and text
    \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
int MessageBoxHandler::information(const QString &identifier, const QString &title,
    const QString &text, int buttons, int button) const
{
    return showMessageBox(informationType, currentBestSuitParent(), identifier, title, text,
        QMessageBox::StandardButtons(buttons), QMessageBox::StandardButton(button));
}

/*!
    Opens a question message box with the identifier \a identifier, title \a title, and text
    \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
int MessageBoxHandler::question(const QString &identifier, const QString &title,
    const QString &text, int buttons, int button) const
{
    return showMessageBox(questionType, currentBestSuitParent(), identifier, title, text,
        QMessageBox::StandardButtons(buttons), QMessageBox::StandardButton(button));
}

/*!
    Opens a warning message box with the identifier \a identifier, title \a title, and text \a text.

    The standard buttons specified by \a buttons are added to the message box. \a button specifies
    the button that is used when \key Enter is pressed. \a button must refer to a button that is
    specified in \a buttons. If \a button is QMessageBox::NoButton, a suitable default is chosen
    automatically.

    Returns the identity of the standard button that was clicked. However, if \key Esc was pressed,
    returns the escape button.
*/
int MessageBoxHandler::warning(const QString &identifier, const QString &title, const QString &text,
    int buttons, int button) const
{
    return showMessageBox(warningType, currentBestSuitParent(), identifier, title, text,
        QMessageBox::StandardButtons(buttons), QMessageBox::StandardButton(button));
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
#if defined(Q_OS_OSX)
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
