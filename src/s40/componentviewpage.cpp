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
#include <QCheckBox>

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

    m_core->setPackageManager();

    setFixedSize(970, 860);
    //resize(970, 860);
    QVBoxLayout *vMainLayout = new QVBoxLayout;
    QHBoxLayout *hMainLayout = new QHBoxLayout;
    QHBoxLayout *hLayoutLeft = new QHBoxLayout;
    QHBoxLayout *hLayoutRight= new QHBoxLayout;
    //creating check box and buttons for sdk manager

    showLabel = new QLabel(QLatin1String("Show:"));
    hLayoutLeft->addWidget(showLabel);

    showInstalled = new QCheckBox(QLatin1String("Installed"));
    showInstalled->setChecked(true);
    hLayoutLeft->addWidget(showInstalled);

    showUpdates = new QCheckBox(QLatin1String("Updates/New"));
    hLayoutLeft->addWidget(showUpdates);


    hMainLayout->addLayout(hLayoutLeft);
    hMainLayout->setAlignment(hLayoutLeft, Qt::AlignLeft);

    hMainLayout->setAlignment(hLayoutRight, Qt::AlignRight);


    m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));
    m_treeView->setModel(m_allModel);
    m_treeView->resizeColumnToContents ( 0 );
    m_treeView->setExpanded(m_allModel->index(0,0), true);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    vMainLayout->addWidget(m_treeView);

    vMainLayout->addLayout(hMainLayout);
    vMainLayout->addSpacing(5);

    QFrame* line = new QFrame;
    line->setObjectName(QString::fromUtf8("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    vMainLayout->addWidget(line);
    vMainLayout->addWidget(m_progressBar);

    progressStatusLabel = new QLabel (QLatin1String("Status:"));
    vMainLayout->addWidget(progressStatusLabel);
    setLayout(vMainLayout);
    //setWindowTitle(QLatin1String("SDK Manager"));


    //signal and slots
    connect(showInstalled, SIGNAL(stateChanged(int)),
            this, SLOT(showInstalledComponents()));
    connect(showUpdates, SIGNAL(stateChanged(int)),
            this, SLOT(showUpdateNewComponents()));

    fetchComponentsData();

    setButtonText(QWizard::NextButton, tr("Install"));


    connect(core, SIGNAL(metaJobInfoMessage(QString)), this, SLOT(setMessage(QString)));
    //connect(core, SIGNAL(coreNetworkSettingsChanged()), this, SLOT(onCoreNetworkSettingsChanged()));
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

/*
 Selects the component with /a id in the component tree.
*/
void ComponentViewPage::selectComponent(const QString &id)
{
    const QModelIndex &idx = m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        m_currentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

/*
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




void ComponentViewPage::setMessage(const QString &msg)
{
    m_label->setText(msg);
}

void ComponentViewPage::setErrorMessage(const QString &error)
{
//    QPalette palette;
//    const PackageManagerCore::Status s = packageManagerCore()->status();
//    if (s == PackageManagerCore::Failure || s == PackageManagerCore::Failure) {
//        palette.setColor(QPalette::WindowText, Qt::red);
//    } else {
//        palette.setColor(QPalette::WindowText, palette.color(QPalette::WindowText));
//    }

//    m_errorLabel->setText(error);
//    m_errorLabel->setPalette(palette);
}


//void ComponentViewPage::onCoreNetworkSettingsChanged()
//{
//    // force a repaint of the ui as after the settings dialog has been closed and the wizard has been
//    // restarted, the "Next" button looks still disabled.   TODO: figure out why this happens at all!
//    gui()->repaint();

//    m_updatesFetched = false;
//    m_allPackagesFetched = false;
//}

void ComponentViewPage::showInstalledComponents()
{//required later
}

void ComponentViewPage::showUpdateNewComponents()
{
    if (showUpdates->checkState()){
        setButtonText(QWizard::NextButton, tr("Update"));
        m_core->setUpdater();
        fetchUpdateComponent();
    }
    else{
    setButtonText(QWizard::NextButton, tr("Install"));
    m_core->setPackageManager();
    m_allPackagesFetched = false;
    fetchComponentsData();
    }
}

void ComponentViewPage::fetchComponentsData()
{
    m_updatesFetched = false;
    // fetch common packages
    bool localPackagesTreeFetched = false;
    if (!m_allPackagesFetched) {
        // first try to fetch the server side packages tree
        m_allPackagesFetched = m_core->fetchRemotePackagesTree();
        if (!m_allPackagesFetched) {
            QString error = m_core->error();
            if (m_core->isPackageManager()) {
                // if that fails and we're in maintenance mode, try to fetch local installed tree
                localPackagesTreeFetched = m_core->fetchLocalPackagesTree();
                if (localPackagesTreeFetched) {
                    // if that succeeded, adjust error message
                    error = QLatin1String("<font color=\"red\">") + error + tr(" Only local package "
                        "management available.") + QLatin1String("</font>");
                }
            }
            setErrorMessage(error);
        }
    }
}


void ComponentViewPage::fetchUpdateComponent()
{
   if (m_core->isUpdater()) {
        if (!m_updatesFetched) {
            m_updatesFetched = m_core->fetchRemotePackagesTree();
            if (!m_updatesFetched)
                setErrorMessage(m_core->error());
        }
    }

}

