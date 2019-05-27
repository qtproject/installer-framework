/**************************************************************************
**
** Copyright (C) 2017 Konstantin Podsvirov <konstantin@podsvirov.pro>
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

#include "updater.h"

#include <QTest>

class tst_CompareVersion : public QObject
{
    Q_OBJECT

private slots:
    void compareVersion();
    void compareVersionX();
    void compareVersionAll();
    void compareVersionExtra();
};

void tst_CompareVersion::compareVersion()
{
    QCOMPARE(KDUpdater::compareVersion("2.0", "2.1"), -1);
    QCOMPARE(KDUpdater::compareVersion("2.1", "2.0"), +1);
    QCOMPARE(KDUpdater::compareVersion("2.0", "2.0"), 0);
    QCOMPARE(KDUpdater::compareVersion("2.1", "2.1"), 0);
    QCOMPARE(KDUpdater::compareVersion("2.1", "2.1-201903190747"), -1);
    QCOMPARE(KDUpdater::compareVersion("2.1-201903190747", "2.1"), +1);
    QCOMPARE(KDUpdater::compareVersion("2.1-201903190747", "2.1-201903190747"), 0);
    QCOMPARE(KDUpdater::compareVersion("2.1-0", "2.1-201903190747"), -1);
    QCOMPARE(KDUpdater::compareVersion("2.1-201903190747", "2.1-0"), +1);
    QCOMPARE(KDUpdater::compareVersion("2.0.12.4", "2.1.10.4"), -1);
}

void tst_CompareVersion::compareVersionX()
{
    QCOMPARE(KDUpdater::compareVersion("2.0", "2.x"), 0);
    QCOMPARE(KDUpdater::compareVersion("2.x", "2.0"), 0);

    QCOMPARE(KDUpdater::compareVersion("2.0.12.x", "2.0.x"), 0);
    QCOMPARE(KDUpdater::compareVersion("2.1.12.x", "2.0.x"), +1);
    QCOMPARE(KDUpdater::compareVersion("2.1.12.x", "2.x"), 0);
    QCOMPARE(KDUpdater::compareVersion("2.x", "2.1.12.x"), 0);
}

void tst_CompareVersion::compareVersionAll()
{
    QCOMPARE(KDUpdater::compareVersion("version-1", "version-2"), -1);

    QCOMPARE(KDUpdater::compareVersion("v2.0", "v2.x"), 0);
    QCOMPARE(KDUpdater::compareVersion("v2.x", "v2.0"), 0);

    QCOMPARE(KDUpdater::compareVersion("v2.0-alpha", "v2.0-alpha"), 0);

    QCOMPARE(KDUpdater::compareVersion("v2.0-alpha", "v2.0-beta"), -1);
    QCOMPARE(KDUpdater::compareVersion("v2.0-beta", "v2.0-alpha"), +1);

    QCOMPARE(KDUpdater::compareVersion("v2.0-rc1", "v2.0-beta"), +1);
    QCOMPARE(KDUpdater::compareVersion("v2.0-rc1", "v2.0-rc2"), -1);
    QCOMPARE(KDUpdater::compareVersion("v2.0-rc2", "v2.0-rc11"), -1);
    QCOMPARE(KDUpdater::compareVersion("v2.0-rc22", "v2.0-rc2"), +1);

    QCOMPARE(KDUpdater::compareVersion("v2.0", "v2.0-rc3"), +1);
}

void tst_CompareVersion::compareVersionExtra()
{
    QCOMPARE(KDUpdater::compareVersion("OpenSSL_1_0_2k", "OpenSSL_1_0_2l"), -1);
    QCOMPARE(KDUpdater::compareVersion("OpenSSL_1_1_0f", "OpenSSL_1_0_2k"), +1);
}

QTEST_MAIN(tst_CompareVersion)

#include "tst_compareversion.moc"
