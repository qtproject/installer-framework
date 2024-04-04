/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#ifndef PROGRESSCOORDINATOR_H
#define PROGRESSCOORDINATOR_H

#include "installer_global.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

namespace QInstaller {

class INSTALLER_EXPORT ProgressCoordinator : public QObject
{
    Q_OBJECT

    struct ProgressSpinner {
        ProgressSpinner()
            : spinnerChars(QLatin1String("/-\\|"))
            , currentIndex(0) {}

        const QString spinnerChars;
        quint8 currentIndex;
    };

public:
    static ProgressCoordinator *instance();
    ~ProgressCoordinator();

    void registerPartProgress(QObject *sender, const char *signal, double partProgressSize);

public slots:
    void reset();
    void setUndoMode();

    QString labelText() const;
    void setLabelText(const QString &text);

    int progressInPercentage() const;
    void partProgressChanged(double fraction);

    void addManualPercentagePoints(int value);
    void addReservePercentagePoints(int value);

    void emitDetailTextChanged(const QString &text);
    void emitLabelAndDetailTextChanged(const QString &text);

    void emitAdditionalProgressStatus(const QString &status);
    void printProgressPercentage(int progress);
    void printProgressMessage(const QString &message);

signals:
    void detailTextChanged(const QString &text);
    void detailTextResetNeeded();
    void additionalProgressStatusChanged(const QString &status);

protected:
    explicit ProgressCoordinator(QObject *parent);

private:
    double allPendingCalculatedPartPercentages(const QString &excludeKeyObject = QString());
    void disconnectAllSenders();

private:
    QHash<QString, double> m_senderPendingCalculatedPercentageHash;
    QHash<QString, double> m_senderPartProgressSizeHash;
    ProgressSpinner *m_progressSpinner;
    QString m_installationLabelText;
    double m_currentCompletePercentage;
    double m_currentBasePercentage;
    int m_manualAddedPercentage;
    int m_reservedPercentage;
    bool m_undoMode;
    double m_reachedPercentageBeforeUndo;
};

} //namespace QInstaller

#endif //PROGRESSCOORDINATOR_H
