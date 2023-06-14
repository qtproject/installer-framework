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

#ifndef OBSERVER_H
#define OBSERVER_H

#include <QCryptographicHash>
#include <QObject>

namespace QInstaller {

class Observer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Observer)

public:
    Observer() {}
    virtual ~Observer() {}

    virtual int progressValue() const = 0;
    virtual QString progressText() const = 0;
};

class FileTaskObserver : public Observer
{
    Q_OBJECT
    Q_DISABLE_COPY(FileTaskObserver)

public:
    FileTaskObserver(QCryptographicHash::Algorithm algorithm);
    ~FileTaskObserver();

    int progressValue() const override;
    QString progressText() const override;

    QByteArray checkSum() const;
    void addCheckSumData(const QByteArray &data);

    void addSample(qint64 sample);
    void timerEvent(QTimerEvent *event) override;

    void setBytesTransfered(qint64 bytesTransfered);
    void addBytesTransfered(qint64 bytesTransfered);
    void setBytesToTransfer(qint64 bytesToTransfer);

private:
    void init();

private:
    int m_timerId;
    int m_timerInterval;

    qint64 m_bytesTransfered;
    qint64 m_bytesToTransfer;

    qint64 m_samples[50];
    quint32 m_sampleIndex;
    qint64 m_bytesPerSecond;
    qint64 m_currentSpeedBin;

    QCryptographicHash m_hash;
};

}   // namespace QInstaller

#endif // OBSERVER_H
