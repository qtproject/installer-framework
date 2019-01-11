/**************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef COMPONENTSELECTIONPAGE_P_H
#define COMPONENTSELECTIONPAGE_P_H

#include <QObject>
#include <QWidget>

#include "componentmodel.h"
#include "packagemanagergui.h"

class QTreeView;
class QLabel;
class QPushButton;
class QGroupBox;
class QListWidgetItem;
class QProgressBar;
class QVBoxLayout;
class QHBoxLayout;

namespace QInstaller {

class PackageManagerCore;
class ComponentModel;
class ComponentSelectionPage;

class ComponentSelectionPagePrivate : public QObject
{
    Q_OBJECT
    friend class ComponentSelectionPage;
    Q_DISABLE_COPY(ComponentSelectionPagePrivate)

public:
    explicit ComponentSelectionPagePrivate(ComponentSelectionPage *qq, PackageManagerCore *core);
    ~ComponentSelectionPagePrivate();

    void allowCompressedRepositoryInstall();
    void showCompressedRepositoryButton();
    void hideCompressedRepositoryButton();
    void setupCategoryLayout();
    void showCategoryLayout(bool show);
    void updateTreeView();

public slots:
    void currentSelectedChanged(const QModelIndex &current);
    void selectAll();
    void deselectAll();
    void checkboxStateChanged();
    void enableRepositoryCategory(const QString &repositoryName, bool enable);
    void updateWidgetVisibility(bool show);
    void fetchRepositoryCategories();
    void customButtonClicked(int which);
    void onProgressChanged(int progress);
    void setMessage(const QString &msg);
    void setTotalProgress(int totalProgress);
    void selectDefault();
    void onModelStateChanged(QInstaller::ComponentModel::ModelState state);

private:
    ComponentSelectionPage *q;
    PackageManagerCore *m_core;
    QTreeView *m_treeView;
    QLabel *m_sizeLabel;
    QLabel *m_descriptionLabel;
    QVBoxLayout *m_descriptionVLayout;
    QPushButton *m_checkAll;
    QPushButton *m_uncheckAll;
    QPushButton *m_checkDefault;
    QWidget *m_categoryWidget;
    QGroupBox *m_categoryGroupBox;
    QLabel *m_metadataProgressLabel;
    QProgressBar *m_progressBar;
    QHBoxLayout *m_mainHLayout;
    QVBoxLayout *m_treeViewVLayout;
    bool m_allowCompressedRepositoryInstall;
    ComponentModel *m_allModel;
    ComponentModel *m_updaterModel;
    ComponentModel *m_currentModel;
};

}  // namespace QInstaller

#endif // COMPONENTSELECTIONPAGE_P_H
