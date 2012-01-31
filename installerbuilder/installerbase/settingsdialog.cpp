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
#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <packagemanagercore.h>

#include <QtGui/QItemSelectionModel>
#include <QtGui/QTreeWidget>


// -- PasswordDelegate

void PasswordDelegate::showPasswords(bool show)
{
    m_showPasswords = show;
}

void PasswordDelegate::disableEditing(bool disable)
{
    m_disabledEditor = disable;
}

QString PasswordDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    const QString tmp = QStyledItemDelegate::displayText(value, locale);
    if (m_showPasswords)
        return tmp;
    return QString(tmp.length(), QChar(0x25CF));
}

QWidget *PasswordDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &)
    const
{
    if (m_disabledEditor)
        return 0;

    QLineEdit *lineEdit = new QLineEdit(parent);
    lineEdit->setEchoMode(m_showPasswords ? QLineEdit::Normal : QLineEdit::Password);
    return lineEdit;
}


// -- RepositoryItem

RepositoryItem::RepositoryItem(const QString &label)
    : QTreeWidgetItem(QTreeWidgetItem::UserType)
{
    setText(0, label);
    m_repo = QInstaller::Repository(QUrl(), true);
}

RepositoryItem::RepositoryItem(const QInstaller::Repository &repo)
    : QTreeWidgetItem(QTreeWidgetItem::UserType)
    , m_repo(repo)
{
    if (!repo.isDefault())
        setFlags(flags() | Qt::ItemIsEditable);
}

QVariant RepositoryItem::data(int column, int role) const
{
    const QVariant &data = QTreeWidgetItem::data(column, role);

    switch (role) {
        case Qt::UserRole: {
            if (column == 0)
                return m_repo.isDefault();
        }   break;

        case Qt::CheckStateRole: {
            if (column == 1)
                return (m_repo.isEnabled() ? Qt::Checked : Qt::Unchecked);
        }   break;

        case Qt::EditRole:
        case Qt::DisplayRole:{
            switch (column) {
                case 0:
                    return data.toString().isEmpty() ? QLatin1String(" ") : data;
                case 2:
                    return m_repo.username();
                case 3:
                    return m_repo.password();
                case 4:
                    return m_repo.url().toString();
                default:
                    break;
            };
        }   break;

        case Qt::ToolTipRole:
            switch (column) {
                case 1:
                    return SettingsDialog::tr("Check this to use repository during fetch.");
                case 2:
                    return SettingsDialog::tr("Add the username to authenticate on the server.");
                case 3:
                    return SettingsDialog::tr("Add the password to authenticate on the server.");
                case 4:
                    return SettingsDialog::tr("The servers URL that contains a valid repository.");
                default:
                    return QVariant();
            }   break;
        break;
    };

    return data;
}

void RepositoryItem::setData(int column, int role, const QVariant &value)
{
    switch (role) {
        case Qt::EditRole: {
            switch (column) {
                case 2:
                    m_repo.setUsername(value.toString());
                    break;
                case 3:
                    m_repo.setPassword(value.toString());
                    break;
                case 4:
                    m_repo.setUrl(value.toUrl());
                    break;
                default:
                    break;
            };
        }   break;

        case Qt::CheckStateRole: {
            if (column == 1)
                m_repo.setEnabled(Qt::CheckState(value.toInt()) == Qt::Checked);
        }   break;

        default:
            break;
    }
    QTreeWidgetItem::setData(column, role, value);
}

QSet<QInstaller::Repository> RepositoryItem::repositories() const
{
    QSet<QInstaller::Repository> set;
    for (int i = 0; i < childCount(); ++i) {
        if (QTreeWidgetItem *item = child(i)) {
            if (item->type() == QTreeWidgetItem::UserType) {
                if (RepositoryItem *repoItem = static_cast<RepositoryItem*> (item))
                    set.insert(repoItem->repository());
            }
        }
    }
    return set;
}


// -- SettingsDialog

SettingsDialog::SettingsDialog(QInstaller::PackageManagerCore *core, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::SettingsDialog)
    , m_core(core)
    , m_showPasswords(false)
{
    m_ui->setupUi(this);
    setupRepositoriesTreeWidget();

    const QInstaller::Settings &settings = m_core->settings();
    switch (settings.proxyType()) {
        case QInstaller::Settings::NoProxy:
            m_ui->m_noProxySettings->setChecked(true);
            break;
        case QInstaller::Settings::SystemProxy:
            m_ui->m_systemProxySettings->setChecked(true);
            break;
        case QInstaller::Settings::UserDefinedProxy:
            m_ui->m_manualProxySettings->setChecked(true);
            break;
        default:
            m_ui->m_noProxySettings->setChecked(true);
            Q_ASSERT_X(false, Q_FUNC_INFO, "Unknown proxy type given!");
    }

    const QNetworkProxy &ftpProxy = settings.ftpProxy();
    m_ui->m_ftpProxy->setText(ftpProxy.hostName());
    m_ui->m_ftpProxyPort->setValue(ftpProxy.port());
    m_ui->m_ftpProxyUser->setText(ftpProxy.user());
    m_ui->m_ftpProxyPass->setText(ftpProxy.password());
    m_ui->m_ftpProxyNeedsAuth->setChecked(!ftpProxy.user().isEmpty() | !ftpProxy.password().isEmpty());

    const QNetworkProxy &httpProxy = settings.httpProxy();
    m_ui->m_httpProxy->setText(httpProxy.hostName());
    m_ui->m_httpProxyPort->setValue(httpProxy.port());
    m_ui->m_httpProxyUser->setText(httpProxy.user());
    m_ui->m_httpProxyPass->setText(httpProxy.password());
    m_ui->m_httpProxyNeedsAuth->setChecked(!httpProxy.user().isEmpty() | !httpProxy.password().isEmpty());

    connect(m_ui->m_addRepository, SIGNAL(clicked()), this, SLOT(addRepository()));
    connect(m_ui->m_showPasswords, SIGNAL(clicked()), this, SLOT(updatePasswords()));
    connect(m_ui->m_removeRepository, SIGNAL(clicked()), this, SLOT(removeRepository()));
    connect(m_ui->m_useTmpRepositories, SIGNAL(clicked(bool)), this, SLOT(useTmpRepositories(bool)));
    connect(m_ui->m_repositoriesView, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
        this, SLOT(currentRepositoryChanged(QTreeWidgetItem*, QTreeWidgetItem*)));

    useTmpRepositories(settings.hasReplacementRepos());
    m_ui->m_useTmpRepositories->setChecked(settings.hasReplacementRepos());
    m_ui->m_repositoriesView->setCurrentItem(m_rootItems.at(settings.hasReplacementRepos()));
}

void SettingsDialog::accept()
{
    bool settingsChanged = false;
    QInstaller::Settings newSettings;
    const QInstaller::Settings &settings = m_core->settings();

    // set possible updated default repositories
    newSettings.setDefaultRepositories((dynamic_cast<RepositoryItem*> (m_rootItems.at(0)))->repositories());
    settingsChanged |= (settings.defaultRepositories() != newSettings.defaultRepositories());

    // set possible new temporary repositories
    newSettings.setTemporaryRepositories((dynamic_cast<RepositoryItem*> (m_rootItems.at(1)))->repositories(),
        m_ui->m_useTmpRepositories->isChecked());
    settingsChanged |= (settings.temporaryRepositories() != newSettings.temporaryRepositories());
    settingsChanged |= (settings.hasReplacementRepos() != newSettings.hasReplacementRepos());

    // set possible new user repositories
    newSettings.setUserRepositories((dynamic_cast<RepositoryItem*> (m_rootItems.at(2)))->repositories());
    settingsChanged |= (settings.userRepositories() != newSettings.userRepositories());

    // update proxy type
    newSettings.setProxyType(QInstaller::Settings::NoProxy);
    if (m_ui->m_systemProxySettings->isChecked())
        newSettings.setProxyType(QInstaller::Settings::SystemProxy);
    else if (m_ui->m_manualProxySettings->isChecked())
        newSettings.setProxyType(QInstaller::Settings::UserDefinedProxy);
    settingsChanged |= settings.proxyType() != newSettings.proxyType();

    if (newSettings.proxyType() == QInstaller::Settings::UserDefinedProxy) {
        // update ftp proxy settings
        newSettings.setFtpProxy(QNetworkProxy(QNetworkProxy::HttpProxy, m_ui->m_ftpProxy->text(),
            m_ui->m_ftpProxyPort->value(), m_ui->m_ftpProxyUser->text(), m_ui->m_ftpProxyPass->text()));
        settingsChanged |= (settings.ftpProxy() != newSettings.ftpProxy());

        // update http proxy settings
        newSettings.setHttpProxy(QNetworkProxy(QNetworkProxy::HttpProxy, m_ui->m_httpProxy->text(),
            m_ui->m_httpProxyPort->value(), m_ui->m_httpProxyUser->text(), m_ui->m_httpProxyPass->text()));
        settingsChanged |= (settings.httpProxy() != newSettings.httpProxy());
    }

    if (settingsChanged)
        emit networkSettingsChanged(newSettings);

    QDialog::accept();
}

// -- private slots

void SettingsDialog::addRepository()
{
    int index = 0;
    QTreeWidgetItem *parent = m_ui->m_repositoriesView->currentItem();
    if (parent && !m_rootItems.contains(parent)) {
        parent = parent->parent();
        index = parent->indexOfChild(m_ui->m_repositoriesView->currentItem());
    }

    if (parent) {
        QInstaller::Repository repository;
        repository.setEnabled(true);
        RepositoryItem *item = new RepositoryItem(repository);
        parent->insertChild(index, item);
        m_ui->m_repositoriesView->editItem(item, 4);
        m_ui->m_repositoriesView->scrollToItem(item);
        m_ui->m_repositoriesView->setCurrentItem(item);
    }
}

void SettingsDialog::updatePasswords()
{
    m_showPasswords = !m_showPasswords;
    m_delegate->showPasswords(m_showPasswords);
    m_ui->m_showPasswords->setText(m_showPasswords ? tr("Hide Passwords") : tr("Show Passwords"));

    // force an tree view update so the delegate has to repaint
    m_ui->m_repositoriesView->viewport()->update();
}

void SettingsDialog::removeRepository()
{
    QTreeWidgetItem *item = m_ui->m_repositoriesView->currentItem();
    if (item && !m_rootItems.contains(item)) {
        QTreeWidgetItem *parent = item->parent();
        if (parent)
            delete parent->takeChild(parent->indexOfChild(item));
    }
}

void SettingsDialog::useTmpRepositories(bool use)
{
    m_rootItems.at(0)->setDisabled(use);
    m_rootItems.at(2)->setDisabled(use);
}

void SettingsDialog::currentRepositoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (current) {
        const int index = m_rootItems.at(0)->indexOfChild(current);
        m_ui->m_removeRepository->setEnabled(!current->data(0, Qt::UserRole).toBool());
        m_ui->m_addRepository->setEnabled((current != m_rootItems.at(0)) & (index == -1));
    }
}

// -- private

void SettingsDialog::setupRepositoriesTreeWidget()
{
    QTreeWidget *treeWidget = m_ui->m_repositoriesView;
    treeWidget->header()->setVisible(true);
    treeWidget->setHeaderLabels(QStringList() << QString() << tr("Use") << tr("Username") << tr("Password")
         << tr("Repository"));
    m_rootItems.append(new RepositoryItem(tr("Default repositories")));
    m_rootItems.append(new RepositoryItem(tr("Temporary repositories")));
    m_rootItems.append(new RepositoryItem(tr("User defined repositories")));
    treeWidget->addTopLevelItems(m_rootItems);

    const QInstaller::Settings &settings = m_core->settings();
    insertRepositories(settings.userRepositories(), m_rootItems.at(2));
    insertRepositories(settings.defaultRepositories(), m_rootItems.at(0));
    insertRepositories(settings.temporaryRepositories(), m_rootItems.at(1));

    treeWidget->expandAll();
    for (int i = 0; i < treeWidget->model()->columnCount(); ++i)
        treeWidget->resizeColumnToContents(i);

    treeWidget->header()->setResizeMode(0, QHeaderView::Fixed);
    treeWidget->header()->setResizeMode(1, QHeaderView::Fixed);
    treeWidget->header()->setMinimumSectionSize(treeWidget->columnWidth(1));
    treeWidget->setItemDelegateForColumn(0, new PasswordDelegate(treeWidget));
    treeWidget->setItemDelegateForColumn(1, new PasswordDelegate(treeWidget));
    treeWidget->setItemDelegateForColumn(3, m_delegate = new PasswordDelegate(treeWidget));
    m_delegate->showPasswords(false);
    m_delegate->disableEditing(false);
}

void SettingsDialog::insertRepositories(const QSet<QInstaller::Repository> repos, QTreeWidgetItem *rootItem)
{
    rootItem->setFirstColumnSpanned(true);
    foreach (const QInstaller::Repository &repo, repos)
        rootItem->addChild(new RepositoryItem(repo));
}
