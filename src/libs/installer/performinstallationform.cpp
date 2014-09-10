/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "performinstallationform.h"

#include "lazyplaintextedit.h"
#include "progresscoordinator.h"

#include <QApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include <QtCore/QTimer>

#ifdef Q_OS_WIN
# include <QWinTaskbarButton>
# include <QWinTaskbarProgress>
#endif

using namespace QInstaller;

// -- PerformInstallationForm

PerformInstallationForm::PerformInstallationForm(QObject *parent)
    : QObject(parent)
    , m_progressBar(0)
    , m_progressLabel(0)
    , m_detailsButton(0)
    , m_detailsBrowser(0)
    , m_updateTimer(0)
{
#ifdef Q_OS_WIN
    m_taskButton = new QWinTaskbarButton(this);
    m_taskButton->progress()->setVisible(true);
#endif
}

void PerformInstallationForm::setupUi(QWidget *widget)
{
    QVBoxLayout *baseLayout = new QVBoxLayout(widget);
    baseLayout->setObjectName(QLatin1String("BaseLayout"));

    QVBoxLayout *topLayout = new QVBoxLayout();
    topLayout->setObjectName(QLatin1String("TopLayout"));

    m_progressBar = new QProgressBar(widget);
    m_progressBar->setRange(1, 100);
    m_progressBar->setObjectName(QLatin1String("ProgressBar"));
    topLayout->addWidget(m_progressBar);

    m_progressLabel = new QLabel(widget);
    m_progressLabel->setObjectName(QLatin1String("ProgressLabel"));
    m_progressLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    topLayout->addWidget(m_progressLabel);

    m_downloadStatus = new QLabel(widget);
    m_downloadStatus->setObjectName(QLatin1String("DownloadStatus"));
    m_downloadStatus->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    topLayout->addWidget(m_downloadStatus);
    connect(ProgressCoordinator::instance(), SIGNAL(downloadStatusChanged(QString)), this,
        SLOT(onDownloadStatusChanged(QString)));

    m_detailsButton = new QPushButton(tr("&Show Details"), widget);
    m_detailsButton->setObjectName(QLatin1String("DetailsButton"));
    m_detailsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(m_detailsButton, SIGNAL(clicked()), this, SLOT(toggleDetails()));
    topLayout->addWidget(m_detailsButton);

    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->setObjectName(QLatin1String("BottomLayout"));
    bottomLayout->addStretch();

    m_detailsBrowser = new LazyPlainTextEdit(widget);
    m_detailsBrowser->setReadOnly(true);
    m_detailsBrowser->setWordWrapMode(QTextOption::NoWrap);
    m_detailsBrowser->setObjectName(QLatin1String("DetailsBrowser"));
    m_detailsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    bottomLayout->addWidget(m_detailsBrowser);

    bottomLayout->setStretch(1, 10);
    baseLayout->addLayout(topLayout);
    baseLayout->addLayout(bottomLayout);

    m_updateTimer = new QTimer(widget);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(updateProgress())); //updateProgress includes label
    m_updateTimer->setInterval(30);

    m_progressBar->setRange(0, 100);
}

void PerformInstallationForm::setDetailsWidgetVisible(bool visible)
{
    m_detailsButton->setVisible(visible);
}

void PerformInstallationForm::appendProgressDetails(const QString &details)
{
    m_detailsBrowser->append(details);
}

void PerformInstallationForm::updateProgress()
{
    QInstaller::ProgressCoordinator *progressCoordninator = QInstaller::ProgressCoordinator::instance();
    const int progressPercentage = progressCoordninator->progressInPercentage();

    m_progressBar->setValue(progressPercentage);
#ifdef Q_OS_WIN
    if (!m_taskButton->window())
        m_taskButton->setWindow(QApplication::activeWindow()->windowHandle());
    m_taskButton->progress()->setValue(progressPercentage);
#endif

    static QString lastLabelText;
    if (lastLabelText == progressCoordninator->labelText())
        return;
    lastLabelText = progressCoordninator->labelText();
    m_progressLabel->setText(m_progressLabel->fontMetrics().elidedText(progressCoordninator->labelText(),
        Qt::ElideRight, m_progressLabel->width()));
}

void PerformInstallationForm::toggleDetails()
{
    const bool willShow = !isShowingDetails();
    m_detailsButton->setText(willShow ? tr("&Hide Details") : tr("&Show Details"));
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
    m_detailsButton->setText(tr("&Show Details"));
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
    m_detailsBrowser->updateCursor(LazyPlainTextEdit::TextCursorPosition::ForceEnd);
}

bool PerformInstallationForm::isShowingDetails() const
{
    return m_detailsBrowser->isVisible();
}

void PerformInstallationForm::onDownloadStatusChanged(const QString &status)
{
    m_downloadStatus->setText(m_downloadStatus->fontMetrics().elidedText(status, Qt::ElideRight,
        m_downloadStatus->width()));
}
