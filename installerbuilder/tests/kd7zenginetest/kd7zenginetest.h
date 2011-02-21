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
#ifndef KD7ZENGINETEST_H
#define KD7ZENGINETEST_H

#include <QObject>

#include <QtTest/QtTest>

class KD7zEngineTest : public QObject {
    Q_OBJECT
public:
    KD7zEngineTest();
private Q_SLOTS:
    void testIsSupportedArchive();
    void testListing();
    void testExtract();
    void testCompress();
    void testFileEngine();
};

#endif // KD7ZENGINETEST_H
