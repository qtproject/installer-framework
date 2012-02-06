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

#ifndef PROGRESSCOORDNINATOR_H
#define PROGRESSCOORDNINATOR_H

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

namespace QInstaller {

class ProgressCoordinator : public QObject
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

#endif //PROGRESSCOORDNINATOR_H
