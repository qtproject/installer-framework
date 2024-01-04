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

#include "componentselectionpage_p.h"

#include "globals.h"
#include "packagemanagergui.h"
#include "componentmodel.h"
#include "settings.h"
#include "component.h"
#include "fileutils.h"
#include "messageboxhandler.h"
#include "customcombobox.h"

#include <QTreeView>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QGroupBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QStandardPaths>
#include <QFileDialog>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QStyledItemDelegate>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ComponentSelectionPagePrivate
    \internal
*/

constexpr int scNoCheckSelectionIndex = -1;
constexpr int scCheckDefaultIndex = 0;
constexpr int scCheckAllIndex = 1;
constexpr int scUncheckAllIndex = 2;

ComponentSelectionPagePrivate::ComponentSelectionPagePrivate(ComponentSelectionPage *qq, PackageManagerCore *core)
        : q(qq)
        , m_core(core)
        , m_treeView(new QTreeView(q))
        , m_tabWidget(nullptr)
        , m_descriptionBaseWidget(nullptr)
        , m_categoryWidget(Q_NULLPTR)
        , m_allowCreateOfflineInstaller(false)
        , m_categoryLayoutVisible(false)
        , m_allModel(m_core->defaultComponentModel())
        , m_updaterModel(m_core->updaterComponentModel())
        , m_currentModel(m_allModel)
        , m_proxyModel(m_core->componentSortFilterProxyModel())
        , m_componentsResolved(false)
        , m_headerStretchLastSection(false)
{
    m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));
    m_treeView->setUniformRowHeights(true);

    m_descriptionBaseWidget = new QWidget(q);
    m_descriptionBaseWidget->setObjectName(QLatin1String("DescriptionBaseWidget"));

    QVBoxLayout *descriptionVLayout = new QVBoxLayout(m_descriptionBaseWidget);
    descriptionVLayout->setObjectName(QLatin1String("DescriptionLayout"));
    descriptionVLayout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget(q);
    m_tabWidget->setObjectName(QLatin1String("ComponentSelectionTabWidget"));
    m_tabWidget->tabBar()->setObjectName(QLatin1String("ComponentSelectionTabBar"));
    m_tabWidget->hide();

    m_rightSideVLayout = new QVBoxLayout;

    QScrollArea *descriptionScrollArea = new QScrollArea(q);
    descriptionScrollArea->setWidgetResizable(true);
    descriptionScrollArea->setFrameShape(QFrame::NoFrame);
    descriptionScrollArea->setObjectName(QLatin1String("DescriptionScrollArea"));

    m_descriptionLabel = new QLabel(m_descriptionBaseWidget);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_descriptionLabel->setOpenExternalLinks(true);
    m_descriptionLabel->setObjectName(QLatin1String("ComponentDescriptionLabel"));
    m_descriptionLabel->setAlignment(Qt::AlignTop);
    descriptionScrollArea->setWidget(m_descriptionLabel);
    descriptionVLayout->addWidget(descriptionScrollArea);

    m_sizeLabel = new QLabel(m_descriptionBaseWidget);
    m_sizeLabel->setWordWrap(true);
    m_sizeLabel->setObjectName(QLatin1String("ComponentSizeLabel"));
    descriptionVLayout->addWidget(m_sizeLabel);

    m_createOfflinePushButton = new QPushButton(q);
    m_createOfflinePushButton->setVisible(false);
    m_createOfflinePushButton->setText(ComponentSelectionPage::tr("Create Offline Installer"));
    m_createOfflinePushButton->setToolTip(
            ComponentSelectionPage::tr("Create offline installer from selected components, instead "
            "of installing now."));

    connect(m_createOfflinePushButton, &QPushButton::clicked,
            this, &ComponentSelectionPagePrivate::createOfflineButtonClicked);
    connect(q, &ComponentSelectionPage::completeChanged,
            this, [&]() { m_createOfflinePushButton->setEnabled(q->isComplete()); });

    m_qbspPushButton = new QPushButton(q);
    m_qbspPushButton->setVisible(false);
    m_qbspPushButton->setText(ComponentSelectionPage::tr("Browse &QBSP files"));
    m_qbspPushButton->setToolTip(
            ComponentSelectionPage::tr("Select a Qt Board Support Package file to install "
            "additional content that is not directly available from the online repositories."));

    connect(m_qbspPushButton, &QPushButton::clicked,
            this, &ComponentSelectionPagePrivate::qbspButtonClicked);

    m_rightSideVLayout->addWidget(m_descriptionBaseWidget);
    m_rightSideVLayout->addWidget(m_createOfflinePushButton);
    m_rightSideVLayout->addWidget(m_qbspPushButton);

    QHBoxLayout *topHLayout = new QHBoxLayout;

    // Using custom combobox to workaround QTBUG-90595
    m_checkStateComboBox = new CustomComboBox(q);
#ifdef Q_OS_MACOS
    QStyledItemDelegate *delegate = new QStyledItemDelegate(this);
    m_checkStateComboBox->setItemDelegate(delegate);
#endif
    m_checkStateComboBox->setObjectName(QLatin1String("CheckStateComboBox"));
    topHLayout->addWidget(m_checkStateComboBox);

    connect(m_checkStateComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComponentSelectionPagePrivate::updateAllCheckStates);

    // Workaround invisible placeholder text
    QPalette palette = m_checkStateComboBox->palette();
    palette.setColor(QPalette::PlaceholderText, palette.color(QPalette::Text));
    m_checkStateComboBox->setPalette(palette);

    m_checkStateComboBox->setPlaceholderText(ComponentSelectionPage::tr("Select"));
    if (m_core->isInstaller()) {
        m_checkStateComboBox->insertItem(scCheckDefaultIndex, ComponentSelectionPage::tr("Default"));
        m_checkStateComboBox->setItemData(scCheckDefaultIndex,
            ComponentSelectionPage::tr("Select default components in the tree view."), Qt::ToolTipRole);
    } else {
        m_checkStateComboBox->insertItem(scCheckDefaultIndex, ComponentSelectionPage::tr("Reset"));
        m_checkStateComboBox->setItemData(scCheckDefaultIndex,
            ComponentSelectionPage::tr("Reset all components to their original selection state in the tree view."), Qt::ToolTipRole);
    }
    m_checkStateComboBox->insertItem(scCheckAllIndex, ComponentSelectionPage::tr("Select All"));
    m_checkStateComboBox->setItemData(scCheckAllIndex,
        ComponentSelectionPage::tr("Select all components in the tree view."), Qt::ToolTipRole);
    m_checkStateComboBox->insertItem(scUncheckAllIndex, ComponentSelectionPage::tr("Deselect All"));
    m_checkStateComboBox->setItemData(scUncheckAllIndex,
        ComponentSelectionPage::tr("Deselect all components in the tree view."), Qt::ToolTipRole);

    QWidget *progressStackedWidget = new QWidget();
    QVBoxLayout *metaLayout = new QVBoxLayout(progressStackedWidget);
    m_metadataProgressLabel = new QLabel(progressStackedWidget);
    m_progressBar = new QProgressBar(progressStackedWidget);
    m_progressBar->setRange(0, 0);
    m_progressBar->setObjectName(QLatin1String("CompressedInstallProgressBar"));
    metaLayout->addSpacing(20);
    metaLayout->addWidget(m_metadataProgressLabel);
    metaLayout->addWidget(m_progressBar);
    metaLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_searchLineEdit = new QLineEdit(q);
    m_searchLineEdit->setObjectName(QLatin1String("SearchLineEdit"));
    m_searchLineEdit->setPlaceholderText(ComponentSelectionPage::tr("Search"));
    m_searchLineEdit->setClearButtonEnabled(true);
    connect(m_searchLineEdit, &QLineEdit::textChanged,
            this, &ComponentSelectionPagePrivate::setSearchPattern);
    connect(q, &ComponentSelectionPage::entered, m_searchLineEdit, &QLineEdit::clear);
    topHLayout->addWidget(m_searchLineEdit);

    QVBoxLayout *treeViewVLayout = new QVBoxLayout;
    treeViewVLayout->setObjectName(QLatin1String("TreeviewLayout"));
    treeViewVLayout->addWidget(m_treeView, 3);

    QWidget *mainStackedWidget = new QWidget();
    m_mainGLayout = new QGridLayout(mainStackedWidget);
    {
        int left = 0;
        int top = 0;
        int bottom = 0;
        m_mainGLayout->getContentsMargins(&left, &top, nullptr, &bottom);
        m_mainGLayout->setContentsMargins(left, top, 0, bottom);
    }
    m_mainGLayout->addLayout(topHLayout, 0, 0);
    m_mainGLayout->addLayout(treeViewVLayout, 1, 0);
    m_mainGLayout->addLayout(m_rightSideVLayout, 0, 1, 0, -1);
    m_mainGLayout->setColumnStretch(0, 3);
    m_mainGLayout->setColumnStretch(1, 2);

    m_stackedLayout = new QStackedLayout(q);
    m_stackedLayout->addWidget(mainStackedWidget);
    m_stackedLayout->addWidget(progressStackedWidget);
    m_stackedLayout->setCurrentIndex(0);

    connect(m_allModel, &ComponentModel::checkStateChanged,
            this, &ComponentSelectionPagePrivate::onModelStateChanged);
    connect(m_updaterModel, &ComponentModel::checkStateChanged,
            this, &ComponentSelectionPagePrivate::onModelStateChanged);

    connect(m_core, SIGNAL(metaJobProgress(int)), this, SLOT(onProgressChanged(int)));
    connect(m_core, SIGNAL(metaJobInfoMessage(QString)), this, SLOT(setMessage(QString)));
    connect(m_core, &PackageManagerCore::metaJobTotalProgress, this,
            &ComponentSelectionPagePrivate::setTotalProgress);
}

ComponentSelectionPagePrivate::~ComponentSelectionPagePrivate()
{

}

void ComponentSelectionPagePrivate::setAllowCreateOfflineInstaller(bool allow)
{
    m_allowCreateOfflineInstaller = allow;
}

void ComponentSelectionPagePrivate::showCompressedRepositoryButton()
{
    if (m_core->allowCompressedRepositoryInstall())
        m_qbspPushButton->setVisible(true);
}

void ComponentSelectionPagePrivate::hideCompressedRepositoryButton()
{
    m_qbspPushButton->setVisible(false);
}

void ComponentSelectionPagePrivate::showCreateOfflineInstallerButton(bool show)
{
    if (show && m_allowCreateOfflineInstaller)
        m_createOfflinePushButton->setVisible(m_core->isInstaller() && !m_core->isOfflineOnly());
    else
        m_createOfflinePushButton->setVisible(false);
}

void ComponentSelectionPagePrivate::setupCategoryLayout()
{
    if (m_categoryWidget)
        return;
    m_categoryWidget = new QWidget();
    QVBoxLayout *vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    m_categoryWidget->setLayout(vLayout);
    m_categoryGroupBox = new QGroupBox(q);
    m_categoryGroupBox->setObjectName(QLatin1String("CategoryGroupBox"));
    QVBoxLayout *categoryLayout = new QVBoxLayout(m_categoryGroupBox);
    QPushButton *fetchCategoryButton = new QPushButton(tr("Filter"));
    fetchCategoryButton->setObjectName(QLatin1String("FetchCategoryButton"));
    fetchCategoryButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    fetchCategoryButton->setToolTip(
        ComponentSelectionPage::tr("Filter the enabled repository categories"));
    connect(fetchCategoryButton, &QPushButton::clicked, this,
            &ComponentSelectionPagePrivate::fetchRepositoryCategories);

    foreach (RepositoryCategory repository, m_core->settings().organizedRepositoryCategories()) {
        QCheckBox *checkBox = new QCheckBox;
        checkBox->setObjectName(repository.displayname());
        checkBox->setChecked(repository.isEnabled());
        checkBox->setText(repository.displayname());
        checkBox->setToolTip(repository.tooltip());
        categoryLayout->addWidget(checkBox);
    }
    categoryLayout->addWidget(fetchCategoryButton);

    vLayout->addWidget(m_categoryGroupBox);
    vLayout->addStretch();
    m_tabWidget->insertTab(1, m_categoryWidget, m_core->settings().repositoryCategoryDisplayName());
}

void ComponentSelectionPagePrivate::showCategoryLayout(bool show)
{
    if (!show && !m_categoryWidget)
        return;

    if (show == m_categoryLayoutVisible)
        return;

    setupCategoryLayout();
    if (show) {
        m_rightSideVLayout->removeWidget(m_descriptionBaseWidget);
        m_tabWidget->insertTab(0, m_descriptionBaseWidget, tr("Information"));
        m_rightSideVLayout->insertWidget(0, m_tabWidget);
    } else {
        m_tabWidget->removeTab(0);
        m_rightSideVLayout->removeWidget(m_tabWidget);
        m_rightSideVLayout->insertWidget(0, m_descriptionBaseWidget);
        m_descriptionBaseWidget->setVisible(true);
    }
    m_tabWidget->setVisible(show);
    m_categoryLayoutVisible = show;
}

void ComponentSelectionPagePrivate::updateTreeView()
{
    setComboBoxItemEnabled(scCheckDefaultIndex, m_core->isInstaller() || m_core->isPackageManager());
    if (m_treeView->selectionModel()) {
        disconnect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ComponentSelectionPagePrivate::currentSelectedChanged);
    }

    m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
    m_proxyModel->setSourceModel(m_currentModel);
    m_treeView->setModel(m_proxyModel);
    expandDefault();

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

    m_treeView->setCurrentIndex(m_proxyModel->index(0, 0));
}

/*!
    Expands components that should be expanded by default.
*/
void ComponentSelectionPagePrivate::expandDefault()
{
    m_treeView->setExpanded(m_proxyModel->index(0, 0), true);
    foreach (auto *component, m_core->components(PackageManagerCore::ComponentType::All)) {
        if (component->isExpandedByDefault()) {
            const QModelIndex index = m_proxyModel->mapFromSource(
                m_currentModel->indexFromComponentName(component->treeName()));
            m_treeView->setExpanded(index, true);
        }
    }
}

/*!
    Expands components that were accepted by proxy models filter.
*/
void ComponentSelectionPagePrivate::expandSearchResults()
{
    // Avoid resizing the sections after each expand of a node
    storeHeaderResizeModes();

    // Expand parents of root indexes accepted by filter
    const QVector<QModelIndex> acceptedIndexes = m_proxyModel->directlyAcceptedIndexes();
    for (auto proxyModelIndex : acceptedIndexes) {
        if (!proxyModelIndex.isValid())
            continue;

        QModelIndex index = proxyModelIndex.parent();
        while (index.isValid()) {
            if (m_treeView->isExpanded(index))
                break; // Multiple direct matches in a branch, can be skipped

            m_treeView->expand(index);
            index = index.parent();
        }
    }
    restoreHeaderResizeModes();
}

/*!
    Returns \c true if the components to install and uninstall are calculated
    successfully, \c false otherwise.
*/
bool ComponentSelectionPagePrivate::componentsResolved() const
{
    return m_componentsResolved;
}

void ComponentSelectionPagePrivate::currentSelectedChanged(const QModelIndex &current)
{
    if (!current.isValid())
        return;

    m_sizeLabel->setText(QString());

    QString description = m_proxyModel->data(m_proxyModel->index(current.row(),
        ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString();

    m_descriptionLabel->setText(description);

    Component *component = m_currentModel->componentFromIndex(m_proxyModel->mapToSource(current));
    if ((m_core->isUninstaller()) || (!component))
        return;

    if (component->isSelected() && (component->value(scUncompressedSizeSum).toLongLong() > 0)) {
        m_sizeLabel->setText(ComponentSelectionPage::tr("This component "
            "will occupy approximately %1 on your hard disk drive.")
            .arg(humanReadableSize(component->value(scUncompressedSizeSum).toLongLong())));
    }
}

/*!
    Updates the checkstate of the components based on the value of \c which.
*/
void ComponentSelectionPagePrivate::updateAllCheckStates(int which)
{
    switch (which) {
    case scNoCheckSelectionIndex:
        // A 'helper' text index, no selection
        return;
    case scCheckDefaultIndex:
        selectDefault();
        break;
    case scCheckAllIndex:
        selectAll();
        break;
    case scUncheckAllIndex:
        deselectAll();
        break;
    default:
        qCWarning(lcInstallerInstallLog) << "Invalid index for check state selection!";
        break;
    }
    // Reset back to 'helper' text index
    m_checkStateComboBox->setCurrentIndex(scNoCheckSelectionIndex);
}

void ComponentSelectionPagePrivate::selectAll()
{
    m_currentModel->setCheckedState(ComponentModel::AllChecked);
}

void ComponentSelectionPagePrivate::deselectAll()
{
    m_currentModel->setCheckedState(ComponentModel::AllUnchecked);
}

void ComponentSelectionPagePrivate::updateWidgetVisibility(bool show)
{
    if (show)
        m_stackedLayout->setCurrentIndex(1);
    else
        m_stackedLayout->setCurrentIndex(0);

    m_qbspPushButton->setEnabled(!show);

    if (show) {
        q->gui()->button(QWizard::NextButton)->setEnabled(false);
        q->gui()->button(QWizard::BackButton)->setEnabled(false);
    }

    // In macOS 10.12 the widgets are not hidden if those are not updated immediately
#ifdef Q_OS_MACOS
    q->repaint();
#endif
}

void ComponentSelectionPagePrivate::fetchRepositoryCategories()
{
    updateWidgetVisibility(true);

    QList<QCheckBox*> checkboxes = m_categoryGroupBox->findChildren<QCheckBox *>();
    for (int i = 0; i < checkboxes.count(); i++) {
        QCheckBox *checkbox = checkboxes.at(i);
        m_core->enableRepositoryCategory(checkbox->objectName(), checkbox->isChecked());
    }

    if (!m_core->fetchRemotePackagesTree()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("FailToFetchPackages"), tr("Error"), m_core->error());
    }
    updateWidgetVisibility(false);
    m_searchLineEdit->text().isEmpty() ? expandDefault() : expandSearchResults();
}

void ComponentSelectionPagePrivate::createOfflineButtonClicked()
{
    m_core->setOfflineGenerator();
    q->gui()->button(QWizard::NextButton)->click();
}

void ComponentSelectionPagePrivate::qbspButtonClicked()
{
    QString defaultDownloadDirectory =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QStringList fileNames = QFileDialog::getOpenFileNames(nullptr,
        ComponentSelectionPage::tr("Open File"),defaultDownloadDirectory,
        QLatin1String("QBSP or 7z Files (*.qbsp *.7z)"));

    if (m_core->addQBspRepositories(fileNames)) {
        updateWidgetVisibility(true);
        if (!m_core->fetchCompressedPackagesTree()) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("FailToFetchPackages"), tr("Error"), m_core->error());
        }
    }
    updateWidgetVisibility(false);
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
    if (state.testFlag(ComponentModel::Empty)) {
        setComboBoxItemEnabled(scCheckAllIndex, false);
        setComboBoxItemEnabled(scUncheckAllIndex, false);
        setComboBoxItemEnabled(scCheckDefaultIndex, false);
        return;
    }

    m_componentsResolved = m_core->recalculateAllComponents();
    if (!m_componentsResolved) {
        const QString error = !m_core->componentsToInstallError().isEmpty()
            ? m_core->componentsToInstallError() : m_core->componentsToUninstallError();
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("CalculateComponentsError"), tr("Error"), error);
    }

    q->setModified(state.testFlag(ComponentModel::DefaultChecked) == false);
    // If all components in the checked list are only checkable when run without forced
    // installation, set ComponentModel::AllUnchecked as well, as we cannot uncheck anything.
    // Helps to keep the UI correct.
    if ((!m_core->noForceInstallation())
        && (m_currentModel->checked() == m_currentModel->uncheckable())) {
            state |= ComponentModel::AllUnchecked;
    }
    // enable the button if the corresponding flag is not set
    setComboBoxItemEnabled(scCheckAllIndex, state.testFlag(ComponentModel::AllChecked) == false);
    setComboBoxItemEnabled(scUncheckAllIndex, state.testFlag(ComponentModel::AllUnchecked) == false);
    setComboBoxItemEnabled(scCheckDefaultIndex, state.testFlag(ComponentModel::DefaultChecked) == false);

    // update the current selected node (important to reflect possible sub-node changes)
    if (m_treeView->selectionModel())
        currentSelectedChanged(m_treeView->selectionModel()->currentIndex());
}

/*!
    Sets the new filter pattern to \a text and expands the tree nodes.
*/
void ComponentSelectionPagePrivate::setSearchPattern(const QString &text)
{
    m_proxyModel->setFilterWildcard(text);

    m_treeView->collapseAll();
    if (text.isEmpty()) {
        // Expand user selection and default expanded, ensure selected is visible
        QModelIndex index = m_treeView->selectionModel()->currentIndex();
        while (index.isValid()) {
            m_treeView->expand(index);
            index = index.parent();
        }
        expandDefault();
        m_treeView->scrollTo(m_treeView->selectionModel()->currentIndex());
    } else {
        expandSearchResults();
    }
}

/*!
    Stores the current resize modes of the tree view header's columns, and sets
    the new resize modes to \c QHeaderView::Fixed.
*/
void ComponentSelectionPagePrivate::storeHeaderResizeModes()
{
    m_headerStretchLastSection = m_treeView->header()->stretchLastSection();
    for (int i = 0; i < ComponentModelHelper::LastColumn; ++i)
        m_headerResizeModes.insert(i, m_treeView->header()->sectionResizeMode(i));

    m_treeView->header()->setStretchLastSection(false);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Fixed);
}

/*!
    Restores the resize modes of the tree view header's columns, that were
    stored when calling \l storeHeaderResizeModes().
*/
void ComponentSelectionPagePrivate::restoreHeaderResizeModes()
{
    m_treeView->header()->setStretchLastSection(m_headerStretchLastSection);
    for (int i = 0; i < ComponentModelHelper::LastColumn; ++i)
        m_treeView->header()->setSectionResizeMode(i, m_headerResizeModes.value(i));
}

/*!
    Sets the enabled state of the combo box item in \a index to \a enabled.
*/
void ComponentSelectionPagePrivate::setComboBoxItemEnabled(int index, bool enabled)
{
    auto *model = qobject_cast<QStandardItemModel *>(m_checkStateComboBox->model());
    if (!model)
        return;

    QStandardItem *item = model->item(index);
    if (!item)
        return;

    item->setEnabled(enabled);
}

}  // namespace QInstaller
