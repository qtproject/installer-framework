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
class QScrollArea;
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
    void setImageFromFileName(const QString &fileName);

private:
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QLabel *m_downloadStatus;
    QScrollArea *m_productImagesScrollArea;
    QLabel *m_productImagesLabel;
    QPushButton *m_detailsButton;
    LazyPlainTextEdit *m_detailsBrowser;
    QTimer *m_updateTimer;

#ifdef Q_OS_WIN
    QWinTaskbarButton *m_taskButton;
#endif
};

} // namespace QInstaller

#endif // PERFORMINSTALLATIONFORM_H
