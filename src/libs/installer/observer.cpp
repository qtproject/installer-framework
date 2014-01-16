/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "observer.h"

#include <fileutils.h>

namespace QInstaller {

FileTaskObserver::FileTaskObserver(QCryptographicHash::Algorithm algorithm)
    : m_hash(algorithm)
{
    init();
}

FileTaskObserver::~FileTaskObserver()
{
    if (m_timerId >= 0)
        killTimer(m_timerId);
}

int FileTaskObserver::progressValue() const
{
    if (m_bytesToTransfer <= 0 || m_bytesTransfered > m_bytesToTransfer)
        return 0;
    return 100 * m_bytesTransfered / m_bytesToTransfer;
}

QString FileTaskObserver::progressText() const
{
    QString progressText;
    if (m_bytesToTransfer > 0) {
        QString bytesReceived = QInstaller::humanReadableSize(m_bytesTransfered);
        const QString bytesToReceive = QInstaller::humanReadableSize(m_bytesToTransfer);

        // remove the unit from the bytesReceived value if bytesToReceive has the same
        const QString tmp = bytesToReceive.mid(bytesToReceive.indexOf(QLatin1Char(' ')));
        if (bytesReceived.endsWith(tmp))
            bytesReceived.chop(tmp.length());

        progressText = bytesReceived + tr(" of ") + bytesToReceive;
    } else {
        if (m_bytesTransfered > 0)
            progressText = QInstaller::humanReadableSize(m_bytesTransfered) + tr(" received.");
    }

    progressText += QLatin1String(" (") + QInstaller::humanReadableSize(m_bytesPerSecond) + tr("/sec")
        + QLatin1Char(')');
    if (m_bytesToTransfer > 0 && m_bytesPerSecond > 0) {
        const qint64 time = (m_bytesToTransfer - m_bytesTransfered) / m_bytesPerSecond;

        int s = time % 60;
        const int d = time / 86400;
        const int h = (time / 3600) - (d * 24);
        const int m = (time / 60) - (d * 1440) - (h * 60);

        QString days;
        if (d > 0)
            days = QString::number(d) + (d < 2 ? tr(" day") : tr(" days")) + QLatin1String(", ");

        QString hours;
        if (h > 0)
            hours = QString::number(h) + (h < 2 ? tr(" hour") : tr(" hours")) + QLatin1String(", ");

        QString minutes;
        if (m > 0)
            minutes = QString::number(m) + (m < 2 ? tr(" minute") : tr(" minutes"));

        QString seconds;
        if (s >= 0 && minutes.isEmpty()) {
            s = (s <= 0 ? 1 : s);
            seconds = QString::number(s) + (s < 2 ? tr(" second") : tr(" seconds"));
        }
        progressText += tr(" - ") + days + hours + minutes + seconds + tr(" remaining.");
    } else {
        progressText += tr(" - unknown time remaining.");
    }

    return progressText;
}

QByteArray FileTaskObserver::checkSum() const
{
    return m_hash.result();
}

void FileTaskObserver::addCheckSumData(const char *data, int length)
{
    m_hash.addData(data, length);
}

void FileTaskObserver::addSample(qint64 sample)
{
    m_currentSpeedBin += sample;
}

void FileTaskObserver::setBytesTransfered(qint64 bytesReceived)
{
    m_bytesTransfered = bytesReceived;
}

void FileTaskObserver::addBytesTransfered(qint64 bytesReceived)
{
    m_bytesTransfered += bytesReceived;
}

void FileTaskObserver::setBytesToTransfer(qint64 bytesToReceive)
{
    m_bytesToTransfer = bytesToReceive;
}


// -- private

void FileTaskObserver::init()
{
    m_hash.reset();
    m_sampleIndex = 0;
    m_bytesTransfered = 0;
    m_bytesToTransfer = 0;
    m_bytesPerSecond = 0;
    m_currentSpeedBin = 0;

    m_timerId = -1;
    m_timerInterval = 100;
    memset(m_samples, 0, sizeof(m_samples));
    m_timerId = startTimer(m_timerInterval);
}

void FileTaskObserver::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    unsigned int windowSize = sizeof(m_samples) / sizeof(qint64);

    // add speed of last time bin to the window
    m_samples[m_sampleIndex % windowSize] = m_currentSpeedBin;
    m_currentSpeedBin = 0;   // reset bin for next time interval

    // advance the sample index
    m_sampleIndex++;
    m_bytesPerSecond = 0;

    // dynamic window size until the window is completely filled
    if (m_sampleIndex < windowSize)
        windowSize = m_sampleIndex;

    for (unsigned int i = 0; i < windowSize; ++i)
        m_bytesPerSecond += m_samples[i];

    m_bytesPerSecond /= windowSize; // computer average
    m_bytesPerSecond *= 1000.0 / m_timerInterval; // rescale to bytes/second
}

}   // namespace QInstaller
