/**************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
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
#include <QHeaderView>

#include "componentmodel.h"
#include "packagemanagergui.h"
#include "componentsortfilterproxymodel.h"

class QTreeView;
class QLabel;
class QScrollArea;
class QPushButton;
class QGroupBox;
class QListWidgetItem;
class QProgressBar;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QStackedLayout;

namespace QInstaller {

class PackageManagerCore;
class ComponentModel;
class ComponentSelectionPage;
class CustomComboBox;

class ComponentSelectionPagePrivate : public QObject
{
    Q_OBJECT
    friend class ComponentSelectionPage;
    Q_DISABLE_COPY(ComponentSelectionPagePrivate)

public:
    explicit ComponentSelectionPagePrivate(ComponentSelectionPage *qq, PackageManagerCore *core);
    ~ComponentSelectionPagePrivate();

    void setAllowCreateOfflineInstaller(bool allow);
    void showCompressedRepositoryButton();
    void hideCompressedRepositoryButton();
    void showCreateOfflineInstallerButton(bool show);
    void setupCategoryLayout();
    void showCategoryLayout(bool show);
    void updateTreeView();
    void expandDefault();
    void expandSearchResults();
    bool componentsResolved() const;

public slots:
    void currentSelectedChanged(const QModelIndex &current);
    void updateAllCheckStates(int which);
    void selectAll();
    void deselectAll();
    void updateWidgetVisibility(bool show);
    void fetchRepositoryCategories();
    void createOfflineButtonClicked();
    void qbspButtonClicked();
    void onProgressChanged(int progress);
    void setMessage(const QString &msg);
    void setTotalProgress(int totalProgress);
    void selectDefault();
    void onModelStateChanged(QInstaller::ComponentModel::ModelState state);
    void setSearchPattern(const QString &text);

private:
    void storeHeaderResizeModes();
    void restoreHeaderResizeModes();
    void setComboBoxItemEnabled(int index, bool enabled);

private:
    ComponentSelectionPage *q;
    PackageManagerCore *m_core;
    QTreeView *m_treeView;
    QTabWidget *m_tabWidget;
    QWidget *m_descriptionBaseWidget;
    QLabel *m_sizeLabel;
    QLabel *m_descriptionLabel;
    QPushButton *m_createOfflinePushButton;
    QPushButton *m_qbspPushButton;
    CustomComboBox *m_checkStateComboBox;
    QWidget *m_categoryWidget;
    QGroupBox *m_categoryGroupBox;
    QLabel *m_metadataProgressLabel;
    QProgressBar *m_progressBar;
    QGridLayout *m_mainGLayout;
    QVBoxLayout *m_rightSideVLayout;
    bool m_allowCreateOfflineInstaller;
    bool m_categoryLayoutVisible;
    ComponentModel *m_allModel;
    ComponentModel *m_updaterModel;
    ComponentModel *m_currentModel;
    QStackedLayout *m_stackedLayout;
    ComponentSortFilterProxyModel *m_proxyModel;
    QLineEdit *m_searchLineEdit;
    bool m_componentsResolved;

    bool m_headerStretchLastSection;
    QHash<int, QHeaderView::ResizeMode> m_headerResizeModes;
};

}  // namespace QInstaller

#endif // COMPONENTSELECTIONPAGE_P_H
