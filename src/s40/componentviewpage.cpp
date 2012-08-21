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
#include "componentviewpage.h"

#include <component.h>
#include <componentmodel.h>
#include <messageboxhandler.h>
#include <packagemanagercore.h>
#include <settings.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QTreeView>
#include <QtGui/QHeaderView>
#include <QtGui/QProgressBar>
#include <QtGui/QRadioButton>
#include <QtGui/QStackedWidget>
#include <QtGui/QVBoxLayout>

using namespace QInstaller;

// -- ComponentViewPage

ComponentViewPage::ComponentViewPage(QInstaller::PackageManagerCore *core)
    : QInstaller::PackageManagerPage(core)
    , m_updatesFetched(false)
    , m_allPackagesFetched(false)
    , m_core(core)
    , m_treeView(new QTreeView(this))
    , m_allModel(m_core->defaultComponentModel())
    , m_updaterModel(m_core->updaterComponentModel())
    , m_currentModel(m_allModel)
{

    setPixmap(QWizard::LogoPixmap, logoPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ComponentViewPage"));
    setTitle(titleForPage(QLatin1String("ComponentViewPage"), tr("Select Components")));



    m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));

    connect(m_allModel, SIGNAL(defaultCheckStateChanged(bool)), this, SLOT(setModified(bool)));
    connect(m_updaterModel, SIGNAL(defaultCheckStateChanged(bool)), this, SLOT(setModified(bool)));

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setObjectName(QLatin1String("ComponentDescriptionLabel"));

    m_sizeLabel = new QLabel(this);
    m_sizeLabel->setWordWrap(true);
    m_sizeLabel->setObjectName(QLatin1String("ComponentSizeLabel"));

    m_checkDefault = new QPushButton;
    connect(m_checkDefault, SIGNAL(clicked()), this, SLOT(selectDefault()));
    connect(m_allModel, SIGNAL(defaultCheckStateChanged(bool)), m_checkDefault, SLOT(setEnabled(bool)));
    const QVariantHash hash = elementsForPage(QLatin1String("ComponentViewPage"));
    if (m_core->isInstaller()) {
        m_checkDefault->setObjectName(QLatin1String("SelectDefaultComponentsButton"));
        m_checkDefault->setShortcut(QKeySequence(ComponentViewPage::tr("Alt+A", "select default components")));
        m_checkDefault->setText(hash.value(QLatin1String("SelectDefaultComponentsButton"), ComponentViewPage::tr("Def&ault"))
            .toString());
    } else {
        m_checkDefault->setEnabled(false);
        m_checkDefault->setObjectName(QLatin1String("ResetComponentsButton"));
        m_checkDefault->setShortcut(QKeySequence(ComponentViewPage::tr("Alt+R", "reset to already installed components")));
        m_checkDefault->setText(hash.value(QLatin1String("ResetComponentsButton"), ComponentViewPage::tr("&Reset")).toString());
    }

    m_checkAll = new QPushButton;
    connect(m_checkAll, SIGNAL(clicked()), this, SLOT(selectAll()));
    m_checkAll->setObjectName(QLatin1String("SelectAllComponentsButton"));
    m_checkAll->setShortcut(QKeySequence(ComponentViewPage::tr("Alt+S", "select all components")));
    m_checkAll->setText(hash.value(QLatin1String("SelectAllComponentsButton"), ComponentViewPage::tr("&Select All")).toString());

    m_uncheckAll = new QPushButton;
    connect(m_uncheckAll, SIGNAL(clicked()), this, SLOT(deselectAll()));
    m_uncheckAll->setObjectName(QLatin1String("DeselectAllComponentsButton"));
    m_uncheckAll->setShortcut(QKeySequence(ComponentViewPage::tr("Alt+D", "deselect all components")));
    m_uncheckAll->setText(hash.value(QLatin1String("DeselectAllComponentsButton"), ComponentViewPage::tr("&Deselect All"))
        .toString());

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);

    gui()->showSettingsButton(true);

    connect(core, SIGNAL(metaJobInfoMessage(QString)), this, SLOT(setMessage(QString)));
    connect(core, SIGNAL(coreNetworkSettingsChanged()), this, SLOT(onCoreNetworkSettingsChanged()));
}

void ComponentViewPage::currentChanged(const QModelIndex &current)
{
    // if there is not selection or the current selected node didn't change, return
    if (!current.isValid() || current != m_treeView->selectionModel()->currentIndex())
        return;

    m_descriptionLabel->setText(m_currentModel->data(m_currentModel->index(current.row(),
        ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString());

    m_sizeLabel->clear();
    if (!m_core->isUninstaller()) {
        Component *component = m_currentModel->componentFromIndex(current);
        if (component && component->updateUncompressedSize() > 0) {
            const QVariantHash hash = elementsForPage(QLatin1String("ComponentViewPage"));
            m_sizeLabel->setText(hash.value(QLatin1String("ComponentSizeLabel"),
                ComponentViewPage::tr("This component will occupy approximately %1 on your hard disk drive.")).toString()
                .arg(m_currentModel->data(m_currentModel->index(current.row(),
                ComponentModelHelper::UncompressedSizeColumn, current.parent())).toString()));
        }
    }
}

ComponentViewPage::~ComponentViewPage()
{
}


// TODO: all *select* function ignore the fact that components can be selected inside the tree view as
//       well, which will result in e.g. a disabled button state as long as "ALL" components not
//       unchecked again.
void ComponentViewPage::selectAll()
{
    m_currentModel->selectAll();

    m_checkAll->setEnabled(false);
    m_uncheckAll->setEnabled(true);
}

void ComponentViewPage::deselectAll()
{
    m_currentModel->deselectAll();

    m_checkAll->setEnabled(true);
    m_uncheckAll->setEnabled(false);
}

void ComponentViewPage::selectDefault()
{
    m_currentModel->selectDefault();

    m_checkAll->setEnabled(true);
    m_uncheckAll->setEnabled(true);
}

/*!
    Selects the component with /a id in the component tree.
*/
void ComponentViewPage::selectComponent(const QString &id)
{
    const QModelIndex &idx = m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        m_currentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

/*!
    Deselects the component with /a id in the component tree.
*/
void ComponentViewPage::deselectComponent(const QString &id)
{
    const QModelIndex &idx = m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        m_currentModel->setData(idx, Qt::Unchecked, Qt::CheckStateRole);
}


void ComponentViewPage::updateTreeView()
{
    m_checkDefault->setVisible(m_core->isInstaller() || m_core->isPackageManager());
    if (m_treeView->selectionModel()) {
        disconnect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(currentChanged(QModelIndex)));
        disconnect(m_currentModel, SIGNAL(checkStateChanged(QModelIndex)), this,
            SLOT(currentChanged(QModelIndex)));
    }

    m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
    m_treeView->setModel(m_currentModel);
    m_treeView->setExpanded(m_currentModel->index(0, 0), true);

    if (m_core->isInstaller()) {
        m_treeView->setHeaderHidden(true);
        for (int i = 1; i < m_currentModel->columnCount(); ++i)
            m_treeView->hideColumn(i);
    } else {
        m_treeView->header()->setStretchLastSection(true);
        for (int i = 0; i < m_currentModel->columnCount(); ++i)
            m_treeView->resizeColumnToContents(i);
    }

    bool hasChildren = false;
    const int rowCount = m_currentModel->rowCount();
    for (int row = 0; row < rowCount && !hasChildren; ++row)
        hasChildren = m_currentModel->hasChildren(m_currentModel->index(row, 0));
    m_treeView->setRootIsDecorated(hasChildren);

    connect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
        this, SLOT(currentChanged(QModelIndex)));
    connect(m_currentModel, SIGNAL(checkStateChanged(QModelIndex)), this,
        SLOT(currentChanged(QModelIndex)));

    m_treeView->setCurrentIndex(m_currentModel->index(0, 0));
}


bool ComponentViewPage::validatePage()
{
    PackageManagerCore *core = packageManagerCore();

    setComplete(false);
    gui()->setSettingsButtonEnabled(false);

    const bool maintanence = core->isUpdater() || core->isPackageManager();
    if (maintanence) {
        showAll();
    } else {
        showMetaInfoUdate();
    }

    // fetch updater packages
    if (core->isUpdater()) {
        if (!m_updatesFetched) {
            m_updatesFetched = core->fetchRemotePackagesTree();
            if (!m_updatesFetched)
                setErrorMessage(core->error());
        }

        callControlScript(QLatin1String("UpdaterSelectedCallback"));

        if (m_updatesFetched) {
            if (core->updaterComponents().count() <= 0)
                setErrorMessage(QLatin1String("<b>") + tr("No updates available.") + QLatin1String("</b>"));
            else
                setComplete(true);
        }
    }

    // fetch common packages
    if (core->isInstaller() || core->isPackageManager()) {
        bool localPackagesTreeFetched = false;
        if (!m_allPackagesFetched) {
            // first try to fetch the server side packages tree
            m_allPackagesFetched = core->fetchRemotePackagesTree();
            if (!m_allPackagesFetched) {
                QString error = core->error();
                if (core->isPackageManager()) {
                    // if that fails and we're in maintenance mode, try to fetch local installed tree
                    localPackagesTreeFetched = core->fetchLocalPackagesTree();
                    if (localPackagesTreeFetched) {
                        // if that succeeded, adjust error message
                        error = QLatin1String("<font color=\"red\">") + error + tr(" Only local package "
                            "management available.") + QLatin1String("</font>");
                    }
                }
                setErrorMessage(error);
            }
        }

        callControlScript(QLatin1String("PackageManagerSelectedCallback"));

        if (m_allPackagesFetched | localPackagesTreeFetched)
            setComplete(true);
    }

    if (maintanence) {
        showMaintenanceTools();
    } else {
        hideAll();
    }
    gui()->setSettingsButtonEnabled(true);

    return isComplete();
}

void ComponentViewPage::showAll()
{
    showWidgets(true);
}

void ComponentViewPage::hideAll()
{
    showWidgets(false);
}

void ComponentViewPage::showMetaInfoUdate()
{
    showWidgets(false);
    m_label->setVisible(true);
    m_progressBar->setVisible(true);
}

void ComponentViewPage::showMaintenanceTools()
{
    showWidgets(true);
    m_label->setVisible(false);
    m_progressBar->setVisible(false);
}

// -- public slots

void ComponentViewPage::setMessage(const QString &msg)
{
    m_label->setText(msg);
}

void ComponentViewPage::setErrorMessage(const QString &error)
{
    QPalette palette;
    const PackageManagerCore::Status s = packageManagerCore()->status();
    if (s == PackageManagerCore::Failure || s == PackageManagerCore::Failure) {
        palette.setColor(QPalette::WindowText, Qt::red);
    } else {
        palette.setColor(QPalette::WindowText, palette.color(QPalette::WindowText));
    }

    m_errorLabel->setText(error);
    m_errorLabel->setPalette(palette);
}

void ComponentViewPage::callControlScript(const QString &callback)
{
    // Initialize the gui. Needs to be done after check repositories as only then the ui can handle
    // hide of pages depending on the components.
    gui()->init();
    gui()->callControlScriptMethod(callback);
}

void ComponentViewPage::onCoreNetworkSettingsChanged()
{
    // force a repaint of the ui as after the settings dialog has been closed and the wizard has been
    // restarted, the "Next" button looks still disabled.   TODO: figure out why this happens at all!
    gui()->repaint();

    m_updatesFetched = false;
    m_allPackagesFetched = false;
}

// -- private

void ComponentViewPage::entering()
{
    setComplete(true);
    showWidgets(false);
    setMessage(QString());
    setErrorMessage(QString());
    setButtonText(QWizard::CancelButton, tr("Quit"));

    PackageManagerCore *core = packageManagerCore();
    if (core->isUninstaller() ||core->isUpdater() || core->isPackageManager()) {
        showMaintenanceTools();
    }
}

void ComponentViewPage::setModified(bool modified)
{
    setComplete(modified);
}

void ComponentViewPage::leaving()
{
    // TODO: force repaint on next page, keeps unpainted after fetch
    QTimer::singleShot(100, gui()->page(nextId()), SLOT(repaint()));
    setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::CancelButton));
}

void ComponentViewPage::showWidgets(bool show)
{
    m_label->setVisible(show);
    m_progressBar->setVisible(show);
}
