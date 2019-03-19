/**************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "componentselectionpage_p.h"

#include "packagemanagergui.h"
#include "componentmodel.h"
#include "settings.h"
#include "component.h"
#include "fileutils.h"
#include "messageboxhandler.h"

#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QHeaderView>
#include <QStandardPaths>
#include <QFileDialog>

namespace QInstaller {

ComponentSelectionPagePrivate::ComponentSelectionPagePrivate(ComponentSelectionPage *qq, PackageManagerCore *core)
        : q(qq)
        , m_core(core)
        , m_treeView(new QTreeView(q))
        , m_allModel(m_core->defaultComponentModel())
        , m_updaterModel(m_core->updaterComponentModel())
        , m_currentModel(m_allModel)
        , m_allowCompressedRepositoryInstall(false)
        , m_categoryWidget(Q_NULLPTR)
{
    m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));

    connect(m_allModel, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)), this,
        SLOT(onModelStateChanged(QInstaller::ComponentModel::ModelState)));
    connect(m_updaterModel, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)),
        this, SLOT(onModelStateChanged(QInstaller::ComponentModel::ModelState)));

    m_descriptionVLayout = new QVBoxLayout;
    m_descriptionVLayout->setObjectName(QLatin1String("DescriptionLayout"));

    m_descriptionLabel = new QLabel(q);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setObjectName(QLatin1String("ComponentDescriptionLabel"));
    m_descriptionVLayout->addWidget(m_descriptionLabel);

    m_sizeLabel = new QLabel(q);
    m_sizeLabel->setWordWrap(true);
    m_sizeLabel->setObjectName(QLatin1String("ComponentSizeLabel"));
    m_descriptionVLayout->addWidget(m_sizeLabel);
    m_descriptionVLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding,
        QSizePolicy::MinimumExpanding));

    m_treeViewVLayout = new QVBoxLayout;
    m_treeViewVLayout->setObjectName(QLatin1String("TreeviewLayout"));

    QHBoxLayout *buttonHLayout = new QHBoxLayout;
    m_checkDefault = new QPushButton;
    connect(m_checkDefault, &QAbstractButton::clicked,
            this, &ComponentSelectionPagePrivate::selectDefault);
    if (m_core->isInstaller()) {
        m_checkDefault->setObjectName(QLatin1String("SelectDefaultComponentsButton"));
        m_checkDefault->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+A",
            "select default components")));
        m_checkDefault->setText(ComponentSelectionPage::tr("Def&ault"));
    } else {
        m_checkDefault->setEnabled(false);
        m_checkDefault->setObjectName(QLatin1String("ResetComponentsButton"));
        m_checkDefault->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+R",
            "reset to already installed components")));
        m_checkDefault->setText(ComponentSelectionPage::tr("&Reset"));
    }
    buttonHLayout->addWidget(m_checkDefault);

    m_checkAll = new QPushButton;
    connect(m_checkAll, &QAbstractButton::clicked,
            this, &ComponentSelectionPagePrivate::selectAll);
    m_checkAll->setObjectName(QLatin1String("SelectAllComponentsButton"));
    m_checkAll->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+S",
        "select all components")));
    m_checkAll->setText(ComponentSelectionPage::tr("&Select All"));
    buttonHLayout->addWidget(m_checkAll);

    m_uncheckAll = new QPushButton;
    connect(m_uncheckAll, &QAbstractButton::clicked,
            this, &ComponentSelectionPagePrivate::deselectAll);
    m_uncheckAll->setObjectName(QLatin1String("DeselectAllComponentsButton"));
    m_uncheckAll->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+D",
        "deselect all components")));
    m_uncheckAll->setText(ComponentSelectionPage::tr("&Deselect All"));
    buttonHLayout->addWidget(m_uncheckAll);

    m_treeViewVLayout->addLayout(buttonHLayout);

    m_metadataProgressLabel = new QLabel();
    m_metadataProgressLabel->hide();
    m_treeViewVLayout->addWidget(m_metadataProgressLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0);
    m_progressBar->hide();
    m_progressBar->setObjectName(QLatin1String("CompressedInstallProgressBar"));
    m_treeViewVLayout->addWidget(m_progressBar);

    connect(m_core, SIGNAL(metaJobProgress(int)), this, SLOT(onProgressChanged(int)));
    connect(m_core, SIGNAL(metaJobInfoMessage(QString)), this, SLOT(setMessage(QString)));
    connect(m_core, &PackageManagerCore::metaJobTotalProgress, this,
            &ComponentSelectionPagePrivate::setTotalProgress);

    m_treeViewVLayout->addWidget(m_treeView, 3);

    m_mainHLayout = new QHBoxLayout(q);
    m_mainHLayout->addLayout(m_treeViewVLayout, 3);
    m_mainHLayout->addLayout(m_descriptionVLayout, 2);

#ifdef INSTALLCOMPRESSED
    allowCompressedRepositoryInstall();
#endif
}

ComponentSelectionPagePrivate::~ComponentSelectionPagePrivate()
{

}

void ComponentSelectionPagePrivate::allowCompressedRepositoryInstall()
{
    m_allowCompressedRepositoryInstall = true;
}

void ComponentSelectionPagePrivate::showCompressedRepositoryButton()
{
    QWizard *wizard = qobject_cast<QWizard*>(m_core->guiObject());
    if (wizard && !(wizard->options() & QWizard::HaveCustomButton2) && m_allowCompressedRepositoryInstall) {
        wizard->setOption(QWizard::HaveCustomButton2, true);
        wizard->setButtonText(QWizard::CustomButton2,
                ComponentSelectionPage::tr("&Browse QBSP files"));
        connect(wizard, &QWizard::customButtonClicked,
                this, &ComponentSelectionPagePrivate::customButtonClicked);
        q->gui()->updateButtonLayout();
    }
}

void ComponentSelectionPagePrivate::hideCompressedRepositoryButton()
{
    QWizard *wizard = qobject_cast<QWizard*>(m_core->guiObject());
    if (wizard && (wizard->options() & QWizard::HaveCustomButton2)) {
        wizard->setOption(QWizard::HaveCustomButton2, false);
        disconnect(wizard, &QWizard::customButtonClicked,
                this, &ComponentSelectionPagePrivate::customButtonClicked);
        q->gui()->updateButtonLayout();
    }
}

void ComponentSelectionPagePrivate::setupCategoryLayout()
{
    if (m_categoryWidget)
        return;
    m_categoryWidget = new QWidget();
    QVBoxLayout *vLayout = new QVBoxLayout;
    m_categoryWidget->setLayout(vLayout);
    m_categoryGroupBox = new QGroupBox(q);
    m_categoryGroupBox->setTitle(m_core->settings().repositoryCategoryDisplayName());
    m_categoryGroupBox->setObjectName(QLatin1String("CategoryGroupBox"));
    QVBoxLayout *categoryLayout = new QVBoxLayout(m_categoryGroupBox);
    QPushButton *fetchCategoryButton = new QPushButton(tr("Refresh"));
    fetchCategoryButton->setObjectName(QLatin1String("FetchCategoryButton"));
    connect(fetchCategoryButton, &QPushButton::clicked, this,
            &ComponentSelectionPagePrivate::fetchRepositoryCategories);

    foreach (RepositoryCategory repository, m_core->settings().organizedRepositoryCategories()) {
        QCheckBox *checkBox = new QCheckBox;
        checkBox->setObjectName(repository.displayname());
        checkBox->setChecked(repository.isEnabled());
        connect(checkBox, &QCheckBox::stateChanged, this,
                &ComponentSelectionPagePrivate::checkboxStateChanged);
        checkBox->setText(repository.displayname());
        checkBox->setToolTip(repository.tooltip());
        categoryLayout->addWidget(checkBox);
    }

    vLayout->addWidget(m_categoryGroupBox);
    vLayout->addWidget(fetchCategoryButton);
    vLayout->addStretch();
    m_mainHLayout->insertWidget(0, m_categoryWidget);
}

void ComponentSelectionPagePrivate::showCategoryLayout(bool show)
{
    if (show) {
        setupCategoryLayout();
    }
    if (m_categoryWidget)
        m_categoryWidget->setVisible(show);
}

void ComponentSelectionPagePrivate::updateTreeView()
{
    m_checkDefault->setVisible(m_core->isInstaller() || m_core->isPackageManager());
    if (m_treeView->selectionModel()) {
        disconnect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ComponentSelectionPagePrivate::currentSelectedChanged);
    }

    m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
    m_treeView->setModel(m_currentModel);
    m_treeView->setExpanded(m_currentModel->index(0, 0), true);
    foreach (Component *component, m_core->components(PackageManagerCore::ComponentType::All)) {
        if (component->isExpandedByDefault()) {
            const QModelIndex index = m_currentModel->indexFromComponentName(component->name());
            m_treeView->setExpanded(index, true);
        }
    }

    const bool installActionColumnVisible = m_core->settings().installActionColumnVisible();
    if (!installActionColumnVisible)
        m_treeView->hideColumn(ComponentModelHelper::ActionColumn);

    m_treeView->header()->setSectionResizeMode(
                ComponentModelHelper::NameColumn, QHeaderView::ResizeToContents);
    if (m_core->isInstaller()) {
        m_treeView->setHeaderHidden(true);
        for (int i = ComponentModelHelper::InstalledVersionColumn; i < m_currentModel->columnCount(); ++i)
            m_treeView->hideColumn(i);

        if (installActionColumnVisible) {
            m_treeView->header()->setStretchLastSection(false);
            m_treeView->header()->setSectionResizeMode(
                        ComponentModelHelper::NameColumn, QHeaderView::Stretch);
            m_treeView->header()->setSectionResizeMode(
                        ComponentModelHelper::ActionColumn, QHeaderView::ResizeToContents);
        }
    } else {
        m_treeView->header()->setStretchLastSection(true);
        if (installActionColumnVisible) {
            m_treeView->header()->setSectionResizeMode(
                        ComponentModelHelper::NameColumn, QHeaderView::Interactive);
            m_treeView->header()->setSectionResizeMode(
                        ComponentModelHelper::ActionColumn, QHeaderView::Interactive);
        }
        for (int i = 0; i < m_currentModel->columnCount(); ++i)
            m_treeView->resizeColumnToContents(i);
    }

    bool hasChildren = false;
    const int rowCount = m_currentModel->rowCount();
    for (int row = 0; row < rowCount && !hasChildren; ++row)
        hasChildren = m_currentModel->hasChildren(m_currentModel->index(row, 0));
    m_treeView->setRootIsDecorated(hasChildren);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &ComponentSelectionPagePrivate::currentSelectedChanged);

    m_treeView->setCurrentIndex(m_currentModel->index(0, 0));
}

void ComponentSelectionPagePrivate::currentSelectedChanged(const QModelIndex &current)
{
    if (!current.isValid())
        return;

    m_sizeLabel->setText(QString());
    m_descriptionLabel->setText(m_currentModel->data(m_currentModel->index(current.row(),
        ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString());

    Component *component = m_currentModel->componentFromIndex(current);
    if ((m_core->isUninstaller()) || (!component))
        return;

    if (component->isSelected() && (component->value(scUncompressedSizeSum).toLongLong() > 0)) {
        m_sizeLabel->setText(ComponentSelectionPage::tr("This component "
            "will occupy approximately %1 on your hard disk drive.")
            .arg(humanReadableSize(component->value(scUncompressedSizeSum).toLongLong())));
    }
}

void ComponentSelectionPagePrivate::selectAll()
{
    m_currentModel->setCheckedState(ComponentModel::AllChecked);
}

void ComponentSelectionPagePrivate::deselectAll()
{
    m_currentModel->setCheckedState(ComponentModel::AllUnchecked);
}

void ComponentSelectionPagePrivate::checkboxStateChanged()
{
    QList<QCheckBox*> checkboxes = m_categoryGroupBox->findChildren<QCheckBox *>();
    bool enableFetchButton = false;
    foreach (QCheckBox *checkbox, checkboxes) {
        if (checkbox->isChecked()) {
            enableFetchButton = true;
            break;
        }
    }
}

void ComponentSelectionPagePrivate::enableRepositoryCategory(const QString &repositoryName, bool enable)
{
    QMap<QString, RepositoryCategory> organizedRepositoryCategories = m_core->settings().organizedRepositoryCategories();

    QMap<QString, RepositoryCategory>::iterator i = organizedRepositoryCategories.find(repositoryName);
    RepositoryCategory repoCategory;
    while (i != organizedRepositoryCategories.end() && i.key() == repositoryName) {
        repoCategory = i.value();
        i++;
    }

    RepositoryCategory replacement = repoCategory;
    replacement.setEnabled(enable);
    QSet<RepositoryCategory> tmpRepoCategories = m_core->settings().repositoryCategories();
    if (tmpRepoCategories.contains(repoCategory)) {
        tmpRepoCategories.remove(repoCategory);
        tmpRepoCategories.insert(replacement);
        m_core->settings().addRepositoryCategories(tmpRepoCategories);
    }
}

void ComponentSelectionPagePrivate::updateWidgetVisibility(bool show)
{
    if (show) {
        QSpacerItem *verticalSpacer2 = new QSpacerItem(0, 0, QSizePolicy::Minimum,
                                                       QSizePolicy::Expanding);
        m_treeViewVLayout->addSpacerItem(verticalSpacer2);
        m_mainHLayout->removeItem(m_descriptionVLayout);
        //Hide next button during category fetch
        QPushButton *const b = qobject_cast<QPushButton *>(q->gui()->button(QWizard::NextButton));
        b->setEnabled(!show);
    } else {
        QSpacerItem *item = m_treeViewVLayout->spacerItem();
        m_treeViewVLayout->removeItem(item);
        m_mainHLayout->addLayout(m_descriptionVLayout, 2);
        //Call completeChanged() to determine if NextButton should be shown or not after category fetch.
        q->completeChanged();
    }
    if (m_categoryWidget)
        m_categoryWidget->setDisabled(show);
    m_progressBar->setVisible(show);
    m_metadataProgressLabel->setVisible(show);

    m_treeView->setVisible(!show);
    m_checkDefault->setVisible(!show);
    m_checkAll->setVisible(!show);
    m_uncheckAll->setVisible(!show);
    m_descriptionLabel->setVisible(!show);
    m_sizeLabel->setVisible(!show);

    if (QAbstractButton *bspButton = q->gui()->button(QWizard::CustomButton2))
        bspButton->setEnabled(!show);
}

void ComponentSelectionPagePrivate::fetchRepositoryCategories()
{
    updateWidgetVisibility(true);

    QCheckBox *checkbox;
    QList<QCheckBox*> checkboxes = m_categoryGroupBox->findChildren<QCheckBox *>();
    for (int i = 0; i < checkboxes.count(); i++) {
        checkbox = checkboxes.at(i);
        enableRepositoryCategory(checkbox->objectName(), checkbox->isChecked());
    }

    if (!m_core->fetchRemotePackagesTree()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("FailToFetchPackages"), tr("Error"), m_core->error());
    }
    updateWidgetVisibility(false);
}

void ComponentSelectionPagePrivate::customButtonClicked(int which)
{
    if (QWizard::WizardButton(which) == QWizard::CustomButton2) {
        QString defaultDownloadDirectory =
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QStringList fileNames = QFileDialog::getOpenFileNames(nullptr,
            ComponentSelectionPage::tr("Open File"),defaultDownloadDirectory,
            QLatin1String("QBSP or 7z Files (*.qbsp *.7z)"));

        QSet<Repository> set;
        foreach (QString fileName, fileNames) {
            Repository repository = Repository::fromUserInput(fileName, true);
            repository.setEnabled(true);
            set.insert(repository);
        }
        if (set.count() > 0) {
            updateWidgetVisibility(true);
            m_core->settings().addTemporaryRepositories(set, false);
            if (!m_core->fetchCompressedPackagesTree()) {
                MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                    QLatin1String("FailToFetchPackages"), tr("Error"), m_core->error());
            }
        }
        updateWidgetVisibility(false);
    }
}

/*!
    Updates the value of \a progress on the progress bar.
*/
void ComponentSelectionPagePrivate::onProgressChanged(int progress)
{
    m_progressBar->setValue(progress);
}

/*!
    Displays the message \a msg on the page.
*/
void ComponentSelectionPagePrivate::setMessage(const QString &msg)
{
    QWizardPage *page = q->gui()->currentPage();
    if (m_metadataProgressLabel && page && page->objectName() == QLatin1String("ComponentSelectionPage"))
        m_metadataProgressLabel->setText(msg);
}

void ComponentSelectionPagePrivate::setTotalProgress(int totalProgress)
{
    if (m_progressBar)
        m_progressBar->setRange(0, totalProgress);
}

void ComponentSelectionPagePrivate::selectDefault()
{
    m_currentModel->setCheckedState(ComponentModel::DefaultChecked);
}

void ComponentSelectionPagePrivate::onModelStateChanged(QInstaller::ComponentModel::ModelState state)
{
    q->setModified(state.testFlag(ComponentModel::DefaultChecked) == false);
    // If all components in the checked list are only checkable when run without forced
    // installation, set ComponentModel::AllUnchecked as well, as we cannot uncheck anything.
    // Helps to keep the UI correct.
    if ((!m_core->noForceInstallation())
        && (m_currentModel->checked() == m_currentModel->uncheckable())) {
            state |= ComponentModel::AllUnchecked;
    }
    // enable the button if the corresponding flag is not set
    m_checkAll->setEnabled(state.testFlag(ComponentModel::AllChecked) == false);
    m_uncheckAll->setEnabled(state.testFlag(ComponentModel::AllUnchecked) == false);
    m_checkDefault->setEnabled(state.testFlag(ComponentModel::DefaultChecked) == false);

    // update the current selected node (important to reflect possible sub-node changes)
    if (m_treeView->selectionModel())
        currentSelectedChanged(m_treeView->selectionModel()->currentIndex());
}

}  // namespace QInstaller
