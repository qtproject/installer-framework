/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef QINSTALLER_MESSAGEBOXHANDLER_H
#define QINSTALLER_MESSAGEBOXHANDLER_H

#include <installer_global.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <QtGui/QMessageBox>

#include <QtScript/QScriptable>

namespace QInstaller {

QScriptValue registerMessageBox(QScriptEngine *scriptEngine);

class INSTALLER_EXPORT MessageBoxHandler : public QObject, private QScriptable
{
    Q_OBJECT

public:
    enum DefaultAction {
        AskUser,
        Accept,
        Reject
    };

    enum MessageType{
        criticalType,
        informationType,
        questionType,
        warningType
    };

    static MessageBoxHandler *instance();
    static QWidget *currentBestSuitParent();

    void setDefaultAction(DefaultAction defaultAction);
    void setAutomaticAnswer(const QString &identifier, QMessageBox::StandardButton answer);

    static QMessageBox::StandardButton critical(QWidget *parent, const QString &identifier,
        const QString &title, const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton);

    static QMessageBox::StandardButton information(QWidget *parent, const QString &identifier,
        const QString &title, const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button=QMessageBox::NoButton);

    static QMessageBox::StandardButton question(QWidget *parent, const QString &identifier,
        const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
        QMessageBox::StandardButton button = QMessageBox::NoButton);

    static QMessageBox::StandardButton warning(QWidget *parent, const QString &identifier,
        const QString &title, const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton);

    Q_INVOKABLE int critical(const QString &identifier, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;

    Q_INVOKABLE int information(const QString &identifier, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;

    Q_INVOKABLE int question(const QString &identifier, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;

    Q_INVOKABLE int warning(const QString &identifier, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton button = QMessageBox::NoButton) const;

private Q_SLOTS:
    //this removes the slot from the script area
    virtual void deleteLater() {
        QObject::deleteLater();
    }

private:
    explicit MessageBoxHandler(QObject *parent);

    QMessageBox::StandardButton autoReply(QMessageBox::StandardButtons buttons) const;
    QMessageBox::StandardButton showMessageBox(MessageType messageType, QWidget *parent,
        const QString &identifier, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const;

private:
    static MessageBoxHandler *m_instance;

    DefaultAction m_defaultAction;
    QList<QMessageBox::Button> m_buttonOrder;
    QHash<QString, QMessageBox::StandardButton> m_automaticAnswers;
};

}

#endif // QINSTALLER_MESSAGEBOXHANDLER_H
