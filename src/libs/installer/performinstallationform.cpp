/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

/*!
    \class QInstaller::PerformInstallationForm
    \inmodule QtInstallerFramework
    \brief The PerformInstallationForm class shows progress information about
     the installation state.

     A progress bar indicates the progress of the installation, update, or
     uninstallation.

     The page contains a button for showing or hiding detailed information
     about the progress in an \e {details browser}. The text on the button
     changes depending on whether the details browser is currently shown or
     hidden.
*/

/*!
    \fn PerformInstallationForm::showDetailsChanged()

    This signal is emitted when the end users select the details button to show
    or hide progress details.
*/

/*!
    Constructs the perform installation UI with \a parent as parent.
*/
PerformInstallationForm::PerformInstallationForm(QObject *parent)
    : QObject(parent)
    , m_progressBar(0)
    , m_progressLabel(0)
    , m_detailsButton(0)
    , m_detailsBrowser(0)
    , m_updateTimer(0)
{
#ifdef Q_OS_WIN
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
        m_taskButton = new QWinTaskbarButton(this);
        m_taskButton->progress()->setVisible(true);
    } else {
        m_taskButton = 0;
    }
#endif
}

/*!
    Sets up the perform installation UI specified by \a widget.
*/
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

/*!
    Shows the details button if \a visible is \c true.
*/
void PerformInstallationForm::setDetailsWidgetVisible(bool visible)
{
    m_detailsButton->setVisible(visible);
}

/*!
    Displays \a details about progress of the installation in the details
    browser.
*/
void PerformInstallationForm::appendProgressDetails(const QString &details)
{
    m_detailsBrowser->append(details);
}

/*!
    Updates the progress of the installation on the progress bar.
*/
void PerformInstallationForm::updateProgress()
{
    QInstaller::ProgressCoordinator *progressCoordninator = QInstaller::ProgressCoordinator::instance();
    const int progressPercentage = progressCoordninator->progressInPercentage();

    m_progressBar->setValue(progressPercentage);
#ifdef Q_OS_WIN
    if (m_taskButton) {
        if (!m_taskButton->window())
            m_taskButton->setWindow(QApplication::activeWindow()->windowHandle());
        m_taskButton->progress()->setValue(progressPercentage);
    }
#endif

    static QString lastLabelText;
    if (lastLabelText == progressCoordninator->labelText())
        return;
    lastLabelText = progressCoordninator->labelText();
    m_progressLabel->setText(m_progressLabel->fontMetrics().elidedText(progressCoordninator->labelText(),
        Qt::ElideRight, m_progressLabel->width()));
}
/*!
    Sets the text of the details button to \uicontrol {Hide Details} or
    \uicontrol {Show Details} depending on whether the details are currently
    shown or hidden. Emits the showDetailsChanged() signal.
*/
void PerformInstallationForm::toggleDetails()
{
    const bool willShow = !isShowingDetails();
    m_detailsButton->setText(willShow ? tr("&Hide Details") : tr("&Show Details"));
    m_detailsBrowser->setVisible(willShow);
    emit showDetailsChanged();
}

/*!
    Clears the contents of the details browser.
*/
void PerformInstallationForm::clearDetailsBrowser()
{
    m_detailsBrowser->clear();
}

/*!
    Enables the details button with the text \uicontrol {Show Details} and hides
    the details browser.
*/
void PerformInstallationForm::enableDetails()
{
    m_detailsButton->setEnabled(true);
    m_detailsButton->setText(tr("&Show Details"));
    m_detailsBrowser->setVisible(false);
}

/*!
    Starts the update progress timer.
*/
void PerformInstallationForm::startUpdateProgress()
{
    m_updateTimer->start();
    updateProgress();
}

/*!
    Stops the update progress timer.
*/
void PerformInstallationForm::stopUpdateProgress()
{
    m_updateTimer->stop();
    updateProgress();
}

/*!
    Enables the details button if \a enable is \c true.
*/
void PerformInstallationForm::setDetailsButtonEnabled(bool enable)
{
    m_detailsButton->setEnabled(enable);
}

/*!
    Scrolls to the bottom of the details browser.
*/
void PerformInstallationForm::scrollDetailsToTheEnd()
{
    m_detailsBrowser->updateCursor(LazyPlainTextEdit::TextCursorPosition::ForceEnd);
}

/*!
    Returns \c true if the details browser is visible.
*/
bool PerformInstallationForm::isShowingDetails() const
{
    return m_detailsBrowser->isVisible();
}

/*!
    Changes the label text according to the changes in the download status
    specified by \a status.
*/
void PerformInstallationForm::onDownloadStatusChanged(const QString &status)
{
    m_downloadStatus->setText(m_downloadStatus->fontMetrics().elidedText(status, Qt::ElideRight,
        m_downloadStatus->width()));
}
