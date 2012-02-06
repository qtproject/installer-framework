/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "messageboxhandler.h"

#include <QtCore/QDebug>

#include <QtGui/QApplication>
#include <QtGui/QPushButton>
#include <QtGui/QDialogButtonBox>

#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>

QScriptValue QInstaller::registerMessageBox(QScriptEngine *scriptEngine)
{
    // register QMessageBox::StandardButton enum in the script connection
    QScriptValue messageBox = scriptEngine->newQObject(MessageBoxHandler::instance());
    messageBox.setProperty(QLatin1String("Ok"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Yes)));
    messageBox.setProperty(QLatin1String("Open"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Open)));
    messageBox.setProperty(QLatin1String("Save"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Save)));
    messageBox.setProperty(QLatin1String("Cancel"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Cancel)));
    messageBox.setProperty(QLatin1String("Close"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Close)));
    messageBox.setProperty(QLatin1String("Discard"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Discard)));
    messageBox.setProperty(QLatin1String("Apply"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Apply)));
    messageBox.setProperty(QLatin1String("Reset"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Reset)));
    messageBox.setProperty(QLatin1String("RestoreDefaults"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::RestoreDefaults)));
    messageBox.setProperty(QLatin1String("Help"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Help)));
    messageBox.setProperty(QLatin1String("SaveAll"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::SaveAll)));
    messageBox.setProperty(QLatin1String("Yes"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Yes)));
    messageBox.setProperty(QLatin1String("YesToAll"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::YesToAll)));
    messageBox.setProperty(QLatin1String("No"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::No)));
    messageBox.setProperty(QLatin1String("NoToAll"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::NoToAll)));
    messageBox.setProperty(QLatin1String("Abort"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Abort)));
    messageBox.setProperty(QLatin1String("Retry"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Retry)));
    messageBox.setProperty(QLatin1String("Ignore"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::Ignore)));
    messageBox.setProperty(QLatin1String("NoButton"),
        scriptEngine->newVariant(static_cast<int>(QMessageBox::NoButton)));
    scriptEngine->globalObject().setProperty(QLatin1String("QMessageBox"), messageBox);

    return messageBox;
}

using namespace QInstaller;

template <typename T>
static QList<T> reversed(const QList<T> &list)
{
    qFatal("This seems to be broken, check this!!!!");
    // TODO: Figure out what should happen here. See setDefaultAction(...).
#if 1
    // Note: This does not what the function name implies???
    QList<T> res = list;
    qCopyBackward(list.begin(), list.end(), res.end());
    return res;
#else
    // Note: This does what the function name implies, but we need to check if this is what we want.
    QList<T> res = list;
    std::reverse(res.begin(), res.end());
    return res;
#endif
}


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
    if (QApplication::type() == QApplication::Tty) {
        return 0;
    }

    if (qApp->activeModalWidget())
        return qApp->activeModalWidget();

    return qApp->activeWindow();
}

void MessageBoxHandler::setDefaultAction(DefaultAction defaultAction)
{
    if (m_defaultAction == defaultAction)
        return;
    m_defaultAction = defaultAction;

    m_buttonOrder.clear();
    if (m_defaultAction != AskUser) {
        m_buttonOrder << QMessageBox::YesToAll << QMessageBox::Yes << QMessageBox::Ok << QMessageBox::Apply
        << QMessageBox::SaveAll << QMessageBox::Save <<QMessageBox::Retry << QMessageBox::Ignore
        << QMessageBox::Help << QMessageBox::RestoreDefaults << QMessageBox::Reset << QMessageBox::Open
        << QMessageBox::Cancel << QMessageBox::Close << QMessageBox::Abort << QMessageBox::Discard
        << QMessageBox::No << QMessageBox::NoToAll;
    }

    if (m_defaultAction == Reject) {
        // If we want to reject everything, we need the lowest button. For example, if Cancel is existing it
        // could use Cancel, but if Close is existing it will use Close.
        m_buttonOrder = reversed(m_buttonOrder);
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
#if defined(Q_WS_MAC)
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

    if (QApplication::type() == QApplication::Tty)
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
