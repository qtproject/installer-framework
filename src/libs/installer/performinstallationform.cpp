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

#include "performinstallationform.h"

#include "progresscoordinator.h"
#include "globals.h"
#include "aspectratiolabel.h"
#include "packagemanagercore.h"
#include "settings.h"
#include "fileutils.h"

#include <QApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QImageReader>
#include <QScrollArea>
#include <QTextEdit>
#include <QFileInfo>

#include <QtCore/QTimer>

#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
# include <QWinTaskbarButton>
# include <QWinTaskbarProgress>
#endif
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
    \fn QInstaller::PerformInstallationForm::showDetailsChanged()

    This signal is emitted when the end users select the details button to show
    or hide progress details.
*/

/*!
    Constructs the perform installation UI with package manager specified by \a core and
    with \a parent as parent.
*/
PerformInstallationForm::PerformInstallationForm(PackageManagerCore *core, QObject *parent)
    : QObject(parent)
    , m_progressBar(nullptr)
    , m_progressLabel(nullptr)
    , m_additionalProgressStatus(nullptr)
    , m_productImagesScrollArea(nullptr)
    , m_productImagesLabel(nullptr)
    , m_detailsButton(nullptr)
    , m_detailsBrowser(nullptr)
    , m_updateTimer(nullptr)
    , m_core(core)
{
#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
        m_taskButton = new QWinTaskbarButton(this);
        m_taskButton->progress()->setVisible(true);
    } else {
        m_taskButton = nullptr;
    }
#endif
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

    m_additionalProgressStatus = new QLabel(widget);
    m_additionalProgressStatus->setObjectName(QLatin1String("DownloadStatus"));
    m_additionalProgressStatus->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_additionalProgressStatus->setWordWrap(true);
    m_additionalProgressStatus->setTextFormat(Qt::TextFormat::RichText);
    topLayout->addWidget(m_additionalProgressStatus);
    connect(ProgressCoordinator::instance(), &ProgressCoordinator::additionalProgressStatusChanged, this,
        &PerformInstallationForm::onAdditionalProgressStatusChanged);

    m_detailsButton = new QPushButton(tr("&Show Details"), widget);
    m_detailsButton->setObjectName(QLatin1String("DetailsButton"));
    m_detailsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(m_detailsButton, &QAbstractButton::clicked, this, &PerformInstallationForm::toggleDetails);
    topLayout->addWidget(m_detailsButton);

    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->setObjectName(QLatin1String("BottomLayout"));
    bottomLayout->addStretch();

    m_productImagesScrollArea = new QScrollArea(widget);
    m_productImagesScrollArea->setObjectName(QLatin1String("ProductImagesScrollArea"));
    m_productImagesScrollArea->setWidgetResizable(true);
    m_productImagesScrollArea->setFrameShape(QFrame::NoFrame);
    m_productImagesScrollArea->setStyleSheet(QLatin1String("background-color:transparent;"));

    m_productImagesLabel = new AspectRatioLabel(widget);
    m_productImagesLabel->setObjectName(QLatin1String("ProductImagesLabel"));

    m_productImagesScrollArea->setWidget(m_productImagesLabel);
    bottomLayout->addWidget(m_productImagesScrollArea);

    m_detailsBrowser = new QTextEdit(widget);
    m_detailsBrowser->setReadOnly(true);
    m_detailsBrowser->setWordWrapMode(QTextOption::NoWrap);
    m_detailsBrowser->setObjectName(QLatin1String("DetailsBrowser"));
    m_detailsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    bottomLayout->addWidget(m_detailsBrowser);

    bottomLayout->setStretch(1, 10);
    bottomLayout->setStretch(2, 10);
    baseLayout->addLayout(topLayout);
    baseLayout->addLayout(bottomLayout);

    m_updateTimer = new QTimer(widget);
    connect(m_updateTimer, &QTimer::timeout,
            this, &PerformInstallationForm::updateProgress); //updateProgress includes label
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_taskButton) {
        if (!m_taskButton->window() && QApplication::activeWindow())
            m_taskButton->setWindow(QApplication::activeWindow()->windowHandle());
        m_taskButton->progress()->setValue(progressPercentage);
    }
#endif
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
    m_productImagesScrollArea->setVisible(!willShow);
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
    m_productImagesScrollArea->setVisible(true);
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
void PerformInstallationForm::onAdditionalProgressStatusChanged(const QString &status)
{
    m_additionalProgressStatus->setText(status);
}

/*!
    Sets currently shown form image specified by \a fileName. When clicking the image,
    \a url is opened in a browser. If the \a url is a reference to a file, it will
    be opened with a suitable application instead of a Web browser. \a url can be empty.
*/
void PerformInstallationForm::setImageFromFileName(const QString &fileName, const QString &url)
{
    QString imagePath = QFileInfo(fileName).isAbsolute()
            ? fileName
            : m_core->settings().value(QLatin1String("Prefix")).toString() + QLatin1Char('/') + fileName;
    QInstaller::replaceHighDpiImage(imagePath);

    if (!QFile::exists(imagePath)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Image file does not exist:" << fileName;
        return;
    }
    QImageReader reader(imagePath);
    QPixmap pixmap = QPixmap::fromImageReader(&reader);
    if (!pixmap.isNull()) {
        m_productImagesLabel->setPixmapAndUrl(pixmap, url);
    } else {
        qCWarning(QInstaller::lcInstallerInstallLog) <<
            QString::fromLatin1("Failed to load image '%1' : %2.").arg(imagePath, reader.errorString());
    }
}
