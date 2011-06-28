/**************************************************************************
**
** This file is part of Nokia Qt SDK**
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
#ifndef PERFORMINSTALLATIONFORM_H
#define PERFORMINSTALLATIONFORM_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QWidget;
QT_END_NAMESPACE

class LazyPlainTextEdit;


namespace QInstaller {

class PerformInstallationForm : public QObject
{
    Q_OBJECT
public:
    PerformInstallationForm(QObject *parent);
    ~PerformInstallationForm();

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
    void appendProgressDetails( const QString &details);
    void updateProgress();
    void toggleDetails();
    void clearDetailsBrowser();

private:
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    QPushButton *m_detailsButton;
    LazyPlainTextEdit *m_detailsBrowser;
    QTimer *m_updateTimer;
};

} //namespace QInstaller

#endif // PERFORMINSTALLATIONFORM_H
