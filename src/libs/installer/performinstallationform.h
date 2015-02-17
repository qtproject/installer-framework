/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef PERFORMINSTALLATIONFORM_H
#define PERFORMINSTALLATIONFORM_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QWidget;
class QWinTaskbarButton;
QT_END_NAMESPACE

class LazyPlainTextEdit;

namespace QInstaller {

class PerformInstallationForm : public QObject
{
    Q_OBJECT

public:
    explicit PerformInstallationForm(QObject *parent);

    void setupUi(QWidget *widget);
    void setDetailsWidgetVisible(bool visible);
    void enableDetails();
    void startUpdateProgress();
    void stopUpdateProgress();
    void setDetailsButtonEnabled(bool enable);
    void scrollDetailsToTheEnd();
    bool isShowingDetails() const;

signals:
    void showDetailsChanged();

public slots:
    void appendProgressDetails(const QString &details);
    void updateProgress();
    void toggleDetails();
    void clearDetailsBrowser();
    void onDownloadStatusChanged(const QString &status);

private:
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QLabel *m_downloadStatus;
    QPushButton *m_detailsButton;
    LazyPlainTextEdit *m_detailsBrowser;
    QTimer *m_updateTimer;

#ifdef Q_OS_WIN
    QWinTaskbarButton *m_taskButton;
#endif
};

} // namespace QInstaller

#endif // PERFORMINSTALLATIONFORM_H
