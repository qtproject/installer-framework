/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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

    void emitDownloadStatus(const QString &status);

signals:
    void detailTextChanged(const QString &text);
    void detailTextResetNeeded();
    void downloadStatusChanged(const QString &status);

protected:
    explicit ProgressCoordinator(QObject *parent);

private:
    double allPendingCalculatedPartPercentages(QObject *excludeKeyObject = 0);
    void disconnectAllSenders();

private:
    QHash<QPointer<QObject>, double> m_senderPendingCalculatedPercentageHash;
    QHash<QPointer<QObject>, double> m_senderPartProgressSizeHash;
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
