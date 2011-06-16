/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
#include "performinstallationform.h"

#include "lazyplaintextedit.h"
#include "progresscoordinator.h"

#include <common/utils.h>

#include <QtGui/QLabel>
#include <QtGui/QProgressBar>
#include <QtGui/QPushButton>
#include <QtGui/QScrollBar>
#include <QtGui/QVBoxLayout>

#include <QtCore/QTimer>

using namespace QInstaller;

PerformInstallationForm::PerformInstallationForm(QObject *parent)
    : QObject(parent),
      m_detailsWidget(0),
      m_detailsButton(0),
      m_progressBar(0),
      m_progressLabel(0),
      m_detailsBrowser(0),
      m_updateTimer(0)
{
}

PerformInstallationForm::~PerformInstallationForm()
{
}

void PerformInstallationForm::setupUi(QWidget *widget)
{
    m_progressBar = new QProgressBar(widget);
    m_progressBar->setObjectName(QLatin1String("ProgressBar"));
    m_progressBar->setRange(1, 100);

    m_progressLabel = new QLabel(widget);
    m_progressLabel->setObjectName(QLatin1String("ProgressLabel"));

    m_detailsButton = new QPushButton(widget);
    connect(m_detailsButton, SIGNAL(clicked()), this, SLOT(toggleDetails()));
    m_detailsButton->setText(tr("Show Details"));
    m_detailsButton->setObjectName(QLatin1String("button"));
    m_detailsBrowser = new LazyPlainTextEdit(widget);
    m_detailsBrowser->setWordWrapMode(QTextOption::NoWrap);
    m_detailsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_progressLabel);

    m_detailsWidget = new QWidget(widget);
    m_detailsWidget->setObjectName(QLatin1String("details"));
    QHBoxLayout* detailsLayout = new QHBoxLayout(m_detailsWidget);
    detailsLayout->addWidget(m_detailsButton);
    detailsLayout->addStretch();
    layout->addWidget(m_detailsWidget);
    layout->addWidget(m_detailsBrowser);

    m_detailsBrowser->setVisible(false);
    layout->addStretch();
    widget->setLayout(layout);

    m_updateTimer = new QTimer(widget);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(updateProgress())); //updateProgress includes label
    m_updateTimer->setInterval(30);
}

void PerformInstallationForm::setDetailsWidgetVisible(bool visible)
{
    m_detailsWidget->setVisible(visible);
}

void PerformInstallationForm::appendProgressDetails(const QString &details)
{
    m_detailsBrowser->append(details);
}

void PerformInstallationForm::updateProgress()
{
    QInstaller::ProgressCoordninator *progressCoordninator = QInstaller::ProgressCoordninator::instance();
    const int progressPercentage = progressCoordninator->progressInPercentage();

    m_progressBar->setRange(0, (progressPercentage == 0) ? 0 : 100);

    if (progressPercentage != m_progressBar->value())
        m_progressBar->setValue(progressPercentage);
    if (m_progressLabel->text() != progressCoordninator->labelText())
        m_progressLabel->setText(progressCoordninator->labelText());
}

void PerformInstallationForm::toggleDetails()
{
    const bool willShow = !isShowingDetails();
    m_detailsButton->setText(willShow ? tr("Hide Details") : tr("Show Details"));

    if (willShow)
        scrollDetailsToTheEnd();

    m_detailsBrowser->setVisible(willShow);
    emit showDetailsChanged();
}

void PerformInstallationForm::clearDetailsBrowser()
{
    m_detailsBrowser->clear();
}

void PerformInstallationForm::enableDetails()
{
    m_detailsButton->setEnabled(true);
    m_detailsButton->setText(QObject::tr("Show Details"));
    m_detailsBrowser->setVisible(false);
}

void PerformInstallationForm::startUpdateProgress()
{
    m_updateTimer->start();
    updateProgress();
}

void PerformInstallationForm::stopUpdateProgress()
{
    m_updateTimer->stop();
    updateProgress();
}

void PerformInstallationForm::setDetailsButtonEnabled(bool enable)
{
    m_detailsButton->setEnabled(enable);
}

void PerformInstallationForm::scrollDetailsToTheEnd()
{
    m_detailsBrowser->horizontalScrollBar()->setValue(0);
    m_detailsBrowser->verticalScrollBar()->setValue(m_detailsBrowser->verticalScrollBar()->maximum());
}

bool PerformInstallationForm::isShowingDetails() const
{
    return m_detailsBrowser->isVisible();
}
