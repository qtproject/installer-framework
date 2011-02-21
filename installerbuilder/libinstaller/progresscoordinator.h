/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef PROGRESSCOORDNINATOR_H
#define PROGRESSCOORDNINATOR_H

#include <QObject>
#include <QHash>
#include <QPointer>

namespace QInstaller {

class ProgressCoordninator : public QObject
{
    Q_OBJECT
public:
    static ProgressCoordninator* instance();
    ~ProgressCoordninator();

    void registerPartProgress(QObject *sender, const char *signal, double partProgressSize);

signals:
    void detailTextChanged(const QString &text);
    void detailTextResetNeeded();
public slots:
    void reset();
    int progressInPercentage() const;

    void setUndoMode();
    void addManualPercentagePoints(int value);
    void addReservePercentagePoints(int value);
    void setLabelText(const QString &text);
    QString labelText() const;
    void emitDetailTextChanged(const QString &text);
    void emitLabelAndDetailTextChanged(const QString &text);
    void partProgressChanged(double fraction);

protected:
    explicit ProgressCoordninator(QObject *parent);
private:
    double allPendingCalculatedPartPercentages(QObject *excludeKeyObject = 0);
    void disconnectAllSenders();

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
