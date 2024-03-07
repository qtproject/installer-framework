/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <packagemanagercore.h>
#include <productkeycheck.h>
#include <testrepository.h>

#include <QtCore/QFile>

#include <QItemSelectionModel>
#include <QMessageBox>
#include <QTreeWidget>

#include <QtXml/QDomDocument>

using namespace QInstaller;

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
        return nullptr;

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

RepositoryItem::RepositoryItem(const Repository &repo)
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
                    return m_repo.displayname();
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
                    return SettingsDialog::tr("The server's URL that contains a valid repository.");
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
                    m_repo.setUrl(QUrl::fromUserInput(value.toString()));
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

QSet<Repository> RepositoryItem::repositories() const
{
    QSet<Repository> set;
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

SettingsDialog::SettingsDialog(PackageManagerCore *core, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::SettingsDialog)
    , m_core(core)
    , m_showPasswords(false)
    , m_cacheCleared(false)
{
    m_ui->setupUi(this);
    setupRepositoriesTreeWidget();

    const Settings &settings = m_core->settings();
    switch (settings.proxyType()) {
        case Settings::NoProxy:
            m_ui->m_noProxySettings->setChecked(true);
            break;
        case Settings::SystemProxy:
            m_ui->m_systemProxySettings->setChecked(true);
            break;
        case Settings::UserDefinedProxy:
            m_ui->m_manualProxySettings->setChecked(true);
            break;
        default:
            m_ui->m_noProxySettings->setChecked(true);
            Q_ASSERT_X(false, Q_FUNC_INFO, "Unknown proxy type given!");
    }

    const QNetworkProxy &ftpProxy = settings.ftpProxy();
    m_ui->m_ftpProxy->setText(ftpProxy.hostName());
    m_ui->m_ftpProxyPort->setValue(ftpProxy.port());

    const QNetworkProxy &httpProxy = settings.httpProxy();
    m_ui->m_httpProxy->setText(httpProxy.hostName());
    m_ui->m_httpProxyPort->setValue(httpProxy.port());

    connect(m_ui->m_addRepository, &QAbstractButton::clicked,
            this, &SettingsDialog::addRepository);
    connect(m_ui->m_showPasswords, &QAbstractButton::clicked,
            this, &SettingsDialog::updatePasswords);
    connect(m_ui->m_removeRepository, &QAbstractButton::clicked,
            this, &SettingsDialog::removeRepository);
    connect(m_ui->m_useTmpRepositories, &QAbstractButton::clicked,
            this, &SettingsDialog::useTmpRepositoriesOnly);
    connect(m_ui->m_repositoriesView, &QTreeWidget::currentItemChanged,
        this, &SettingsDialog::currentRepositoryChanged);
    connect(m_ui->m_testRepository, &QAbstractButton::clicked,
            this, &SettingsDialog::testRepository);
    connect(m_ui->m_selectAll, &QAbstractButton::clicked,
            this, &SettingsDialog::selectAll);
    connect(m_ui->m_deselectAll, &QAbstractButton::clicked,
            this, &SettingsDialog::deselectAll);

    connect(m_ui->m_clearPushButton, &QAbstractButton::clicked,
            this, &SettingsDialog::clearLocalCacheClicked);
    connect(m_ui->m_clearPushButton, &QAbstractButton::clicked, this, [&] {
        // Disable the button as the new settings will only take effect after
        // closing the dialog.
        m_ui->m_clearPushButton->setEnabled(false);
        m_cacheCleared = true;
    });
    connect(m_ui->m_cachePathLineEdit, &QLineEdit::textChanged, this, [&] {
        if (!m_cacheCleared) {
            // Disable the button if the path is modified between applying settings
            m_ui->m_clearPushButton->setEnabled(
                settings.localCachePath() == m_ui->m_cachePathLineEdit->text());
        }
    });

    useTmpRepositoriesOnly(settings.hasReplacementRepos());
    m_ui->m_useTmpRepositories->setChecked(settings.hasReplacementRepos());
    m_ui->m_useTmpRepositories->setEnabled(settings.hasReplacementRepos());
    m_ui->m_repositoriesView->setCurrentItem(m_rootItems.at(settings.hasReplacementRepos()));

    if (!settings.repositorySettingsPageVisible()) {
        // workaround a inconvenience that the page won't hide inside a QTabWidget
        m_ui->m_repositories->setParent(this);
        m_ui->m_repositories->setVisible(settings.repositorySettingsPageVisible());
    }

    m_ui->m_cachePathLineEdit->setText(settings.localCachePath());
    m_ui->m_clearPushButton->setEnabled(m_core->isValidCache());
    showClearCacheProgress(false);
}

void SettingsDialog::showClearCacheProgress(bool show)
{
    m_ui->m_clearCacheProgressLabel->setVisible(show);
    m_ui->m_clearCacheProgressBar->setVisible(show);
}

void SettingsDialog::accept()
{
    bool settingsChanged = false;
    Settings newSettings;
    const Settings &settings = m_core->settings();

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
    newSettings.setProxyType(Settings::NoProxy);
    if (m_ui->m_systemProxySettings->isChecked())
        newSettings.setProxyType(Settings::SystemProxy);
    else if (m_ui->m_manualProxySettings->isChecked())
        newSettings.setProxyType(Settings::UserDefinedProxy);
    settingsChanged |= settings.proxyType() != newSettings.proxyType();

    if (newSettings.proxyType() == Settings::UserDefinedProxy) {
        // update ftp proxy settings
        newSettings.setFtpProxy(QNetworkProxy(QNetworkProxy::HttpProxy, m_ui->m_ftpProxy->text(),
            m_ui->m_ftpProxyPort->value()));
        settingsChanged |= (settings.ftpProxy() != newSettings.ftpProxy());

        // update http proxy settings
        newSettings.setHttpProxy(QNetworkProxy(QNetworkProxy::HttpProxy, m_ui->m_httpProxy->text(),
            m_ui->m_httpProxyPort->value()));
        settingsChanged |= (settings.httpProxy() != newSettings.httpProxy());
    }

    // need to fetch metadata again
    settingsChanged |= m_cacheCleared;
    m_cacheCleared = false;

    // update cache path
    newSettings.setLocalCachePath(m_ui->m_cachePathLineEdit->text());
    settingsChanged |= (settings.localCachePath() != newSettings.localCachePath());

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
        Repository repository;
        repository.setEnabled(true);
        RepositoryItem *item = new RepositoryItem(repository);
        parent->insertChild(index, item);
        m_ui->m_repositoriesView->editItem(item, 4);
        m_ui->m_repositoriesView->scrollToItem(item);
        m_ui->m_repositoriesView->setCurrentItem(item);

        if (parent == m_rootItems.value(1))
            m_ui->m_useTmpRepositories->setEnabled(parent->childCount() > 0);
    }
}

void SettingsDialog::testRepository()
{
    RepositoryItem *current = dynamic_cast<RepositoryItem*> (m_ui->m_repositoriesView->currentItem());
    if (current && !m_rootItems.contains(current)) {
        m_ui->tabWidget->setEnabled(false);
        m_ui->buttonBox->setEnabled(false);

        TestRepository testJob(m_core);
        testJob.setRepository(current->repository());
        testJob.start();
        testJob.waitForFinished();
        current->setRepository(testJob.repository());

        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowModality(Qt::WindowModal);
        msgBox.setDetailedText(testJob.errorString());

        const bool isError = (testJob.error() > Job::NoError);
        const bool isEnabled = current->data(1, Qt::CheckStateRole).toBool();

        msgBox.setText(isError
            ? tr("An error occurred while testing this repository.")
            : tr("The repository was tested successfully."));

        const bool showQuestion = (isError == isEnabled);
        msgBox.setStandardButtons(showQuestion ? QMessageBox::Yes | QMessageBox::No
            : QMessageBox::Close);
        msgBox.setDefaultButton(showQuestion ? QMessageBox::Yes : QMessageBox::Close);
        if (showQuestion) {
            msgBox.setInformativeText(isEnabled
                ? tr("Do you want to disable the repository?")
                : tr("Do you want to enable the repository?")
            );
        }
        if (msgBox.exec() == QMessageBox::Yes)
            current->setData(1, Qt::CheckStateRole, (!isEnabled) ? Qt::Checked : Qt::Unchecked);

        m_ui->tabWidget->setEnabled(true);
        m_ui->buttonBox->setEnabled(true);
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
        if (parent) {
            delete parent->takeChild(parent->indexOfChild(item));
            if (parent == m_rootItems.value(1) && parent->childCount() <= 0) {
                useTmpRepositoriesOnly(false);
                m_ui->m_useTmpRepositories->setChecked(false);
                m_ui->m_useTmpRepositories->setEnabled(false);
            }
        }
    }
}

void SettingsDialog::useTmpRepositoriesOnly(bool use)
{
    m_rootItems.at(0)->setDisabled(use);
    m_rootItems.at(2)->setDisabled(use);
}

void SettingsDialog::currentRepositoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (current) {
        const int index = m_rootItems.at(0)->indexOfChild(current);
        m_ui->m_testRepository->setEnabled(!m_rootItems.contains(current));
        m_ui->m_removeRepository->setEnabled(!current->data(0, Qt::UserRole).toBool());
        m_ui->m_addRepository->setEnabled((current != m_rootItems.at(0)) & (index == -1));
    }
}
void SettingsDialog::checkSubTree(QTreeWidgetItem *item, Qt::CheckState state)
{
    if (item->flags() & Qt::ItemIsUserCheckable)
        item->setData(1, Qt::CheckStateRole, state);
    for (int i = 0; i < item->childCount(); i++)
        checkSubTree(item->child(i), state);
}

void SettingsDialog::selectAll()
{
    for (int i = 0; i < m_ui->m_repositoriesView->topLevelItemCount(); i++)
        checkSubTree(m_ui->m_repositoriesView->topLevelItem(i), Qt::Checked);
}

void SettingsDialog::deselectAll()
{
    for (int i = 0; i < m_ui->m_repositoriesView->topLevelItemCount(); i++)
        checkSubTree(m_ui->m_repositoriesView->topLevelItem(i), Qt::Unchecked);
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

    const Settings &settings = m_core->settings();
    insertRepositories(settings.userRepositories(), m_rootItems.at(2));
    insertRepositories(settings.defaultRepositories(), m_rootItems.at(0));
    insertRepositories(settings.temporaryRepositories(), m_rootItems.at(1));

    treeWidget->expandAll();
    for (int i = 0; i < treeWidget->model()->columnCount(); ++i)
        treeWidget->resizeColumnToContents(i);

    treeWidget->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);

    treeWidget->header()->setMinimumSectionSize(treeWidget->columnWidth(1));
    treeWidget->setItemDelegateForColumn(0, new PasswordDelegate(treeWidget));
    treeWidget->setItemDelegateForColumn(1, new PasswordDelegate(treeWidget));
    treeWidget->setItemDelegateForColumn(3, m_delegate = new PasswordDelegate(treeWidget));
    m_delegate->showPasswords(false);
    m_delegate->disableEditing(false);
}

void SettingsDialog::insertRepositories(const QSet<Repository> repos, QTreeWidgetItem *rootItem)
{
    rootItem->setFirstColumnSpanned(true);
    foreach (const Repository &repo, repos) {
        RepositoryItem *item = new RepositoryItem(repo);
        rootItem->addChild(item);
        item->setHidden(!ProductKeyCheck::instance()->isValidRepository(repo));
    }
}
