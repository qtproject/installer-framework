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

#ifndef QINSTALLER_MESSAGEBOXHANDLER_H
#define QINSTALLER_MESSAGEBOXHANDLER_H

#include <installer_global.h>

#include <QHash>
#include <QMessageBox>
#include <QObject>

namespace QInstaller {

class INSTALLER_EXPORT MessageBoxHandler : public QObject
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
        int buttons = QMessageBox::Ok, int button = QMessageBox::NoButton) const;

    Q_INVOKABLE int information(const QString &identifier, const QString &title, const QString &text,
        int buttons = QMessageBox::Ok, int button = QMessageBox::NoButton) const;

    Q_INVOKABLE int question(const QString &identifier, const QString &title, const QString &text,
        int buttons = QMessageBox::Yes | QMessageBox::No, int button = QMessageBox::NoButton) const;

    Q_INVOKABLE int warning(const QString &identifier, const QString &title, const QString &text,
        int buttons = QMessageBox::Ok, int button = QMessageBox::NoButton) const;

    static QList<QMessageBox::Button> orderedButtons();

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
Q_DECLARE_METATYPE(QMessageBox::StandardButton)
Q_DECLARE_METATYPE(QMessageBox::StandardButtons)

#endif // QINSTALLER_MESSAGEBOXHANDLER_H
