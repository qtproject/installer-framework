/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QSETTINGSWRAPPER_H
#define QSETTINGSWRAPPER_H

#include <installer_global.h>

#include <QtCore/QObject>
#include <QtCore/QVariant>

class INSTALLER_EXPORT QSettingsWrapper : public QObject
{
    Q_OBJECT

public:
    enum Format {
        NativeFormat,
        IniFormat,
        InvalidFormat
    };

    enum Status {
        NoError,
        AccessError,
        FormatError
    };

    enum Scope {
        UserScope,
        SystemScope
    };

    explicit QSettingsWrapper(QObject *parent = 0);
    explicit QSettingsWrapper(const QString &organization, const QString &application = QString(),
        QObject *parent = 0);
    QSettingsWrapper(const QString &fileName, QSettingsWrapper::Format format, QObject *parent = 0);
    QSettingsWrapper(QSettingsWrapper::Scope scope, const QString &organization,
        const QString &application = QString(), QObject *parent = 0);
    QSettingsWrapper(QSettingsWrapper::Format format, QSettingsWrapper::Scope scope,
        const QString &organization, const QString &application = QString(), QObject *parent = 0);
    ~QSettingsWrapper();

    QStringList allKeys() const;
    QString applicationName() const;
    void beginGroup(const QString &prefix);
    int beginReadArray(const QString &prefix);
    void beginWriteArray(const QString &prefix, int size = -1);
    QStringList childGroups() const;
    QStringList childKeys() const;
    void clear();
    bool contains(const QString &key) const;
    void endArray();
    void endGroup();
    bool fallbacksEnabled() const;
    QString fileName() const;
    QSettingsWrapper::Format format() const;
    QString group() const;
    QTextCodec* iniCodec() const;
    bool isWritable() const;
    QString organizationName() const;
    void remove(const QString &key);
    QSettingsWrapper::Scope scope() const;
    void setArrayIndex(int i);
    void setFallbacksEnabled(bool b);
    void setIniCodec(QTextCodec *codec);
    void setIniCodec(const char *codecName);
    void setValue(const QString &key, const QVariant &value);
    QSettingsWrapper::Status status() const;
    void sync();
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;

private:
    class Private;
    Private *d;
};

#endif  // QSETTINGSWRAPPER_H
