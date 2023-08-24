/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#ifndef VERIFYINSTALLER_H
#define VERIFYINSTALLER_H

#include <packagemanagercore.h>

#include <QString>
#include <QStringList>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QtTest/QTest>

#include <iostream>
#include <sstream>

struct VerifyInstaller
{
    static void verifyInstallerResources(const QString &installDir, const QString &componentName, const QString &fileName)
    {
        QDir dir(installDir + QDir::separator() + "installerResources" + QDir::separator() + componentName);
        QVERIFY2(dir.exists(), qPrintable(QLatin1String("Directory: \"%1\" does not exist").arg(dir.absolutePath())));
        QFileInfo fileInfo;
        fileInfo.setFile(dir, fileName);
        QVERIFY2(fileInfo.exists(), qPrintable(QLatin1String("File: \"%1\" does not exist for \"%2\".")
                .arg(fileName).arg(componentName)));
    }

    static void verifyInstallerResourcesDeletion(const QString &installDir, const QString &componentName)
    {
        QDir dir(installDir + QDir::separator() + "installerResources" + QDir::separator() + componentName);
        QVERIFY2(!dir.exists(), qPrintable(QLatin1String("Directory: \"%1\" is not deleted.").arg(dir.absolutePath())));
    }

    static void verifyInstallerResourceFileDeletion(const QString &installDir, const QString &componentName, const QString &fileName)
    {
        QDir dir(installDir + QDir::separator() + "installerResources" + QDir::separator() + componentName);
        QFileInfo fileInfo;
        fileInfo.setFile(dir, fileName);
        QVERIFY2(!fileInfo.exists(), qPrintable(QLatin1String("File: \"%1\" still exists for \"%2\".")
                                               .arg(fileName).arg(componentName)));
    }

    static void verifyFileExistence(const QString &installDir, const QStringList &fileList)
    {
        for (int i = 0; i < fileList.count(); i++) {
            bool fileExists = QFileInfo::exists(installDir + QDir::separator() + fileList.at(i));
            QVERIFY2(fileExists, QString("File \"%1\" does not exist.").arg(fileList.at(i)).toLatin1());
        }

        QDir dir(installDir);
        QCOMPARE(dir.entryList(QStringList() << "*.*", QDir::Files).count(), fileList.count());
    }

    static QString fileContent(const QString &fileName)
    {
        QFile file(fileName);
        QTextStream stream(&file);
        file.open(QIODevice::ReadOnly);
        QString str = stream.readAll();
        file.close();
        return str;
    }

    static void verifyFileContent(const QString &fileName, const QString &content)
    {
        QVERIFY(fileContent(fileName).contains(content));
    }

    static void verifyFileHasNoContent(const QString &fileName, const QString &content)
    {
        QVERIFY(!fileContent(fileName).contains(content));
    }

    static void addToFileMap(const QDir &baseDir, const QFileInfo &fileInfo, QMap<QString, QByteArray> &map)
    {
        QDir directory(fileInfo.absoluteFilePath());
        directory.setFilter(QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::AllDirs | QDir::Files);
        QFileInfoList fileInfoList = directory.entryInfoList();

        foreach (const QFileInfo &info, fileInfoList) {
            if (info.isDir()) {
                map.insert(baseDir.relativeFilePath(info.filePath()), QByteArray());
                addToFileMap(baseDir, info, map);
            } else {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                QFile file(info.absoluteFilePath());
                QVERIFY(file.open(QIODevice::ReadOnly));
                QVERIFY(hash.addData(&file));
                map.insert(baseDir.relativeFilePath(info.filePath()), hash.result().toHex());
                file.close();
            }
        }
    }

    template <typename Func, typename... Args>
    static void verifyListPackagesMessage(QInstaller::PackageManagerCore *core, const QString &message,
                                   Func func, Args... args)
    {
        std::ostringstream stream;
        std::streambuf *buf = std::cout.rdbuf();
        std::cout.rdbuf(stream.rdbuf());

        (core->*func)(std::forward<Args>(args)...);

        std::cout.rdbuf(buf);
        QVERIFY(stream && stream.tellp() == message.size());
        for (const QString &line : message.split(QLatin1String("\n")))
            QVERIFY(stream.str().find(line.toStdString()) != std::string::npos);
    }
};
#endif
