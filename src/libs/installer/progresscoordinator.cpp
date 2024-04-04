/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "progresscoordinator.h"

#include <iostream>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

#include "globals.h"
#include "utils.h"
#include "loggingutils.h"

using namespace QInstaller;

QT_BEGIN_NAMESPACE
hashValue qHash(QPointer<QObject> key)
{
    return qHash(key.data());
}
QT_END_NAMESPACE

ProgressCoordinator::ProgressCoordinator(QObject *parent)
    : QObject(parent)
    , m_currentCompletePercentage(0)
    , m_currentBasePercentage(0)
    , m_manualAddedPercentage(0)
    , m_reservedPercentage(0)
    , m_undoMode(false)
    , m_reachedPercentageBeforeUndo(0)
{
    // it has to be in the main thread to be able refresh the ui with processEvents
    Q_ASSERT(thread() == qApp->thread());
    m_progressSpinner = new ProgressSpinner();
}

ProgressCoordinator::~ProgressCoordinator()
{
    delete m_progressSpinner;
    m_progressSpinner = nullptr;
}

ProgressCoordinator *ProgressCoordinator::instance()
{
    static ProgressCoordinator *instance =nullptr;
    if (instance == nullptr)
        instance = new ProgressCoordinator(qApp);
    return instance;
}

void ProgressCoordinator::reset()
{
    m_senderPartProgressSizeHash.clear();
    m_senderPendingCalculatedPercentageHash.clear();
    m_installationLabelText.clear();
    m_currentCompletePercentage = 0;
    m_currentBasePercentage = 0;
    m_manualAddedPercentage = 0;
    m_reservedPercentage = 0;
    m_undoMode = false;
    m_reachedPercentageBeforeUndo = 0;
    emit detailTextResetNeeded();
}

void ProgressCoordinator::registerPartProgress(QObject *sender, const char *signal, double partProgressSize)
{
    Q_ASSERT(sender);
    Q_ASSERT(!sender->objectName().isEmpty());
    Q_ASSERT(QString::fromLatin1(signal).contains(QLatin1String("(double)")));
    Q_ASSERT(partProgressSize <= 1);

    m_senderPartProgressSizeHash.insert(sender->objectName(), partProgressSize);
    bool isConnected = connect(sender, signal, this, SLOT(partProgressChanged(double)));
    Q_UNUSED(isConnected);
    Q_ASSERT(isConnected);
}


/*!
    This slot gets the progress changed signals from different tasks. The \a fraction values 0 and 1 are handled as
    special values.

    0 - is just ignored, so you can use a timer which gives the progress, e.g. like a downloader does.
    1 - means the task is finished, even if there comes another 1 from that task, so it will be ignored.
*/
void ProgressCoordinator::partProgressChanged(double fraction)
{
    if (fraction < 0 || fraction > 1) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "The fraction is outside from possible value:"
            << fraction;
        return;
    }

    // no fraction no change
    if (fraction == 0 || !sender())
        return;
    QString senderObjectName = sender()->objectName();

    // ignore senders sending 100% multiple times
    if (fraction == 1 && m_senderPendingCalculatedPercentageHash.contains(senderObjectName)
        && m_senderPendingCalculatedPercentageHash.value(senderObjectName) == 0) {
        return;
    }

    double partProgressSize = m_senderPartProgressSizeHash.value(senderObjectName, 0);
    if (partProgressSize == 0) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "It seems that this sender was not registered "
            "in the right way:" << sender();
        return;
    }

    if (m_undoMode) {
        double maxSize = m_reachedPercentageBeforeUndo * partProgressSize;
        double pendingCalculatedPartPercentage = maxSize * fraction;

         // allPendingCalculatedPartPercentages has negative values
        double newCurrentCompletePercentage = m_currentBasePercentage - pendingCalculatedPartPercentage
            + allPendingCalculatedPartPercentages(senderObjectName);

        //we can't check this here, because some round issues can make it little bit under 0 or over 100
        //Q_ASSERT(newCurrentCompletePercentage >= 0);
        //Q_ASSERT(newCurrentCompletePercentage <= 100);
        if (newCurrentCompletePercentage < 0) {
            qCDebug(QInstaller::lcDeveloperBuild) << newCurrentCompletePercentage << "is smaller than 0 "
                "- this should not happen more than once";
            newCurrentCompletePercentage = 0;
        }
        if (newCurrentCompletePercentage > 100) {
            qCDebug(QInstaller::lcDeveloperBuild) << newCurrentCompletePercentage << "is bigger than 100 "
                "- this should not happen more than once";
            newCurrentCompletePercentage = 100;
        }

        // In undo mode, the progress has to go backward, new has to be smaller than current
        if (qRound(m_currentCompletePercentage) < qRound(newCurrentCompletePercentage)) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "Something is wrong with the calculation "
                "of the progress.";
        }

        m_currentCompletePercentage = newCurrentCompletePercentage;
        if (fraction == 1) {
            m_currentBasePercentage = m_currentBasePercentage - pendingCalculatedPartPercentage;
            m_senderPendingCalculatedPercentageHash.insert(senderObjectName, 0);
        } else {
            m_senderPendingCalculatedPercentageHash.insert(senderObjectName, pendingCalculatedPartPercentage);
        }

    } else { //if (m_undoMode)
        int availablePercentagePoints = 100 - m_manualAddedPercentage - m_reservedPercentage;
        double pendingCalculatedPartPercentage = availablePercentagePoints * partProgressSize * fraction;
        //double checkValue = allPendingCalculatedPartPercentages(sender());

        double newCurrentCompletePercentage = m_manualAddedPercentage + m_currentBasePercentage
            + pendingCalculatedPartPercentage + allPendingCalculatedPartPercentages(senderObjectName);

        //we can't check this here, because some round issues can make it little bit under 0 or over 100
        //Q_ASSERT(newCurrentCompletePercentage >= 0);
        //Q_ASSERT(newCurrentCompletePercentage <= 100);
        if (newCurrentCompletePercentage < 0) {
            qCDebug(QInstaller::lcDeveloperBuild) << newCurrentCompletePercentage << "is smaller than 0 "
                "- this should not happen more than once";
            newCurrentCompletePercentage = 0;
        }

        if (newCurrentCompletePercentage > 100) {
            qCDebug(QInstaller::lcDeveloperBuild) << newCurrentCompletePercentage << "is bigger than 100 "
                "- this should not happen more than once";
            newCurrentCompletePercentage = 100;
        }

        // In normal mode, the progress has to go forward, new has to be larger than current
        if (qRound(m_currentCompletePercentage) > qRound(newCurrentCompletePercentage))
            qCWarning(QInstaller::lcInstallerInstallLog) << "Something is wrong with the calculation of the progress.";

        m_currentCompletePercentage = newCurrentCompletePercentage;

        if (fraction == 1) {
            m_currentBasePercentage = m_currentBasePercentage + pendingCalculatedPartPercentage;
            m_senderPendingCalculatedPercentageHash.insert(senderObjectName, 0);
        } else {
            m_senderPendingCalculatedPercentageHash.insert(senderObjectName, pendingCalculatedPartPercentage);
        }
    } //if (m_undoMode)
    printProgressPercentage(progressInPercentage());
}


/*!
    Contains the installation progress percentage.
*/
int ProgressCoordinator::progressInPercentage() const
{
    int currentValue = qRound(m_currentCompletePercentage);
    Q_ASSERT( currentValue <= 100);
    Q_ASSERT( currentValue >= 0);
    return currentValue;
}

void ProgressCoordinator::setUndoMode()
{
    Q_ASSERT(!m_undoMode);
    m_undoMode = true;

    m_senderPartProgressSizeHash.clear();
    m_senderPendingCalculatedPercentageHash.clear();
    m_reachedPercentageBeforeUndo = progressInPercentage();
    m_currentBasePercentage = m_reachedPercentageBeforeUndo;
}

void ProgressCoordinator::addManualPercentagePoints(int value)
{
    m_manualAddedPercentage = m_manualAddedPercentage + value;
    if (m_undoMode) {
        //we don't do other things in the undomode, maybe later if the last percentage point comes to early
        return;
    }

    m_currentCompletePercentage = m_currentCompletePercentage + value;
    if (m_currentCompletePercentage > 100.0)
        m_currentCompletePercentage = 100.0;

    qApp->processEvents(); //makes the result available in the ui
}

void ProgressCoordinator::addReservePercentagePoints(int value)
{
    m_reservedPercentage = m_reservedPercentage + value;
}

void ProgressCoordinator::setLabelText(const QString &text)
{
    if (m_installationLabelText == text)
        return;
    m_installationLabelText = text;

    // Refresh both message & progress percentage on console
    // when the label text is changed
    printProgressMessage(text);
    printProgressPercentage(progressInPercentage());
}

/*!
    Contains the installation progress label text.
*/
QString ProgressCoordinator::labelText() const
{
    return m_installationLabelText;
}

void ProgressCoordinator::emitDetailTextChanged(const QString &text)
{
    emit detailTextChanged(text);
}

void ProgressCoordinator::emitLabelAndDetailTextChanged(const QString &text)
{
    emit detailTextChanged(text);
    setLabelText(QString(text).remove(QLatin1String("\n")));
    qApp->processEvents(); //makes the result available in the ui
}

double ProgressCoordinator::allPendingCalculatedPartPercentages(const QString &excludeKeyObject)
{
    double result = 0;
    QHash<QString, double>::iterator it = m_senderPendingCalculatedPercentageHash.begin();
    while (it != m_senderPendingCalculatedPercentageHash.end()) {
        if (it.key() != excludeKeyObject)
            result += it.value();
        it++;
    }
    return result;
}

void ProgressCoordinator::emitAdditionalProgressStatus(const QString &status)
{
    emit additionalProgressStatusChanged(status);
}

void ProgressCoordinator::printProgressPercentage(int progress)
{
    if (!LoggingHandler::instance().isVerbose())
        return;

    Q_ASSERT(m_progressSpinner->currentIndex < m_progressSpinner->spinnerChars.size());

    QString formatted = QString::fromLatin1("[%1 %2%]").arg(
        m_progressSpinner->spinnerChars.at(m_progressSpinner->currentIndex), QString::number(progress));

    qCDebug(QInstaller::lcProgressIndicator).noquote() << formatted;

    m_progressSpinner->currentIndex == (m_progressSpinner->spinnerChars.size() - 1)
        ? m_progressSpinner->currentIndex = 0
        : m_progressSpinner->currentIndex++;
}

void ProgressCoordinator::printProgressMessage(const QString &message)
{
    qCDebug(QInstaller::lcInstallerInstallLog).nospace().noquote() << message;
}
