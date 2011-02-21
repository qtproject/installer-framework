/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef QINSTALLER_MESSAGEBOXHANDLER_H
#define QINSTALLER_MESSAGEBOXHANDLER_H

#include <installer_global.h>

#include <QObject>
#include <QScriptable>
#include <QMessageBox>
#include <QHash>

class QString;

namespace QInstaller {

QScriptValue registerMessageBox( QScriptEngine* scriptEngine );

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

    ~MessageBoxHandler();
    static MessageBoxHandler* instance();
    static QWidget* currentBestSuitParent();

    void setAutomaticAnswer(const QString& identifier, QMessageBox::StandardButton answer);
    void setDefaultAction(DefaultAction defaultAction);

    static QMessageBox::StandardButton critical(QWidget* parent, const QString& identifier, const QString& title, const QString& text,
                                         QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                         QMessageBox::StandardButton button = QMessageBox::NoButton );
    static QMessageBox::StandardButton information(QWidget* parent, const QString& identifier, const QString& title, const QString& text,
                                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                            QMessageBox::StandardButton button=QMessageBox::NoButton );
    static QMessageBox::StandardButton question(QWidget* parent, const QString& identifier, const QString& title, const QString& text,
                                         QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No,
                                         QMessageBox::StandardButton button = QMessageBox::NoButton );
    static QMessageBox::StandardButton warning(QWidget* parent, const QString& identifier, const QString& title, const QString& text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                        QMessageBox::StandardButton button = QMessageBox::NoButton);

    Q_INVOKABLE int critical(const QString& identifier, const QString& title, const QString& text,
                                                     QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                                     QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    Q_INVOKABLE int information(const QString& identifier, const QString& title, const QString& text,
                                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                                        QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    Q_INVOKABLE int question(const QString& identifier, const QString& title, const QString& text,
                                                     QMessageBox::StandardButtons buttons = QMessageBox::Yes|QMessageBox::No,
                                                     QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    Q_INVOKABLE int warning(const QString& identifier, const QString& title, const QString& text,
                                                    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                                    QMessageBox::StandardButton button = QMessageBox::NoButton) const;
//this removes the slot from the script area
private Q_SLOTS:
    virtual void deleteLater()  {QObject::deleteLater();}

private:
    MessageBoxHandler(QObject *parent);
    QMessageBox::StandardButton showMessageBox(MessageType messageType, QWidget* parent, const QString& identifier, const QString& title, const QString& text,
                                               QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                               QMessageBox::StandardButton button = QMessageBox::NoButton) const;
    QMessageBox::StandardButton autoReply( QMessageBox::StandardButtons buttons ) const;

    static MessageBoxHandler *m_instance;
    QList<QMessageBox::Button> m_buttonOrder;
    DefaultAction m_defaultAction;
    QHash<QString,QMessageBox::StandardButton> m_automaticAnswers;
};

}

#endif // QINSTALLER_MESSAGEBOXHANDLER_H
