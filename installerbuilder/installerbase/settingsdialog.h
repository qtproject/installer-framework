/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** Nokia at info@qt.nokia.com.
**
**************************************************************************/
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <common/repository.h>
#include <settings.h>

#include <QtGui/QDialog>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QTreeWidgetItem>

QT_BEGIN_NAMESPACE
class QLocale;
class QVariant;
QT_END_NAMESPACE

namespace Ui {
    class SettingsDialog;
}

namespace QInstaller {
    class PackageManagerCore;
}

// -- PasswordDelegate

class PasswordDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    PasswordDelegate(QWidget *parent = 0)
        : QStyledItemDelegate(parent)
        , m_showPasswords(true)
        , m_disabledEditor(true)
    {}

    void showPasswords(bool show);
    void disableEditing(bool disable);

protected:
    QString displayText(const QVariant &value, const QLocale &locale) const;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const;

private:
    bool m_showPasswords;
    bool m_disabledEditor;
};


// -- RepositoryItem

class RepositoryItem : public QTreeWidgetItem
{
public:
    RepositoryItem(const QString &label);
    RepositoryItem(const QInstaller::Repository &repo);

    QVariant data(int column, int role) const;
    void setData(int column, int role, const QVariant &value);

    QSet<QInstaller::Repository> repositories() const;
    QInstaller::Repository repository() const { return m_repo; }
    void setRepository(const QInstaller::Repository &repo) { m_repo = repo; }

private:
    QInstaller::Repository m_repo;
};


// -- SettingsDialog

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(QInstaller::PackageManagerCore *core, QWidget *parent = 0);

public slots:
    void accept();

signals:
    void networkSettingsChanged(const QInstaller::Settings &settings);

private slots:
    void addRepository();
    void updatePasswords();
    void removeRepository();
    void useTmpRepositories(bool use);
    void currentRepositoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    void setupRepositoriesTreeWidget();
    void insertRepositories(const QSet<QInstaller::Repository> repos, QTreeWidgetItem *rootItem);

private:
    Ui::SettingsDialog *m_ui;
    PasswordDelegate *m_delegate;
    QInstaller::PackageManagerCore *m_core;

    bool m_showPasswords;
    QList<QTreeWidgetItem*> m_rootItems;
};

#endif  // SETTINGSDIALOG_H
