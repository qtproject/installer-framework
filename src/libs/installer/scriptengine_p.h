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

#ifndef SCRIPTENGINE_P_H
#define SCRIPTENGINE_P_H

#include "globals.h"

#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJSValue>

namespace QInstaller {

class PackageManagerCore;
class PackageManagerGui;
class ScriptEngine;

class QFileDialogProxy : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QFileDialogProxy)

public:
    QFileDialogProxy(PackageManagerCore *core);

public slots :
    QString getExistingDirectory(const QString &caption, const QString &dir,
            const QString &identifier = QLatin1String("GetExistingDirectory"));

    QString getOpenFileName(const QString &caption, const QString &dir, const QString &filter,
            const QString &identifier = QLatin1String("GetExistingFile"));

private:
    QString getExistingFileOrDirectory(const QString &caption, const QString &identifier,
                                       bool isDirectory);

private:
    PackageManagerCore *m_core;
};

class QDesktopServicesProxy : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QDesktopServicesProxy)

public:
    QDesktopServicesProxy(ScriptEngine *engine)
        : m_engine(engine){}

public slots :
    bool openUrl(const QString &url) const {
        QString urlToOpen = url;
        urlToOpen.replace(QLatin1String("\\\\"), QLatin1String("/"));
        urlToOpen.replace(QLatin1String("\\"), QLatin1String("/"));
        return QDesktopServices::openUrl(QUrl::fromUserInput(urlToOpen));
    }
    QString displayName(qint32 location) const {
        return QStandardPaths::displayName(QStandardPaths::StandardLocation(location));
    }
    QString storageLocation(qint32 location) const {
        return QStandardPaths::writableLocation(QStandardPaths::StandardLocation(location));
    }
    QJSValue findFiles(const QString &path, const QString &pattern);

private:
    void findRecursion(const QString &path, const QString &pattern, QStringList *result);

private:
    ScriptEngine *m_engine;
};

class GuiProxy : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GuiProxy)

public:
    GuiProxy(ScriptEngine *engine, QObject *parent);
    void setPackageManagerGui(PackageManagerGui *gui);

    Q_INVOKABLE QJSValue pageById(int id) const;
    Q_INVOKABLE QJSValue pageByObjectName(const QString &name) const;

    Q_INVOKABLE QJSValue currentPageWidget() const;
    Q_INVOKABLE QJSValue pageWidgetByObjectName(const QString &name) const;

    Q_INVOKABLE QString defaultButtonText(int wizardButton) const;
    Q_INVOKABLE void clickButton(int wizardButton, int delayInMs = 0);
    Q_INVOKABLE void clickButton(const QString &objectName, int delayInMs = 0) const;
    Q_INVOKABLE bool isButtonEnabled(int wizardButton);
    Q_INVOKABLE void setWizardPageButtonText(int pageId, int buttonId, const QString &buttonText);

    Q_INVOKABLE void showSettingsButton(bool show);
    Q_INVOKABLE void setSettingsButtonEnabled(bool enable);

    Q_INVOKABLE QJSValue findChild(QObject *parent, const QString &objectName);
    Q_INVOKABLE QList<QJSValue> findChildren(QObject *parent, const QString &objectName);

    Q_INVOKABLE void setSilent(bool silent);

    Q_INVOKABLE void setTextItems(QObject *object, const QStringList &items);

signals:
    void interrupted();
    void languageChanged();
    void finishButtonClicked();
    void gotRestarted();
    void settingsButtonClicked();

public slots:
    void cancelButtonClicked();
    void reject();
    void rejectWithoutPrompt();
    void showFinishedPage();
    void setModified(bool value);

private:
    ScriptEngine *m_engine;
    PackageManagerGui *m_gui;
};

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::QFileDialogProxy*)
Q_DECLARE_METATYPE(QInstaller::QDesktopServicesProxy*)

#endif // SCRIPTENGINE_H
