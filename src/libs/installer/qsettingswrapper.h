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

#ifndef QSETTINGSWRAPPER_H
#define QSETTINGSWRAPPER_H

#include "protocol.h"
#include "remoteobject.h"

#include <QVariant>

namespace QInstaller {

class INSTALLER_EXPORT QSettingsWrapper : public RemoteObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QSettingsWrapper)

public:
    enum Status {
        NoError = 0,
        AccessError,
        FormatError
    };

    enum Format {
        NativeFormat,
        IniFormat,
        InvalidFormat = 16
    };

    enum Scope {
        UserScope,
        SystemScope
    };

    explicit QSettingsWrapper(const QString &organization,
        const QString &application = QString(), QObject *parent = 0);
    QSettingsWrapper(Scope scope, const QString &organization,
        const QString &application = QString(), QObject *parent = 0);
    QSettingsWrapper(Format format, Scope scope, const QString &organization,
        const QString &application = QString(), QObject *parent = 0);
    QSettingsWrapper(const QString &fileName, Format format, QObject *parent = 0);
    ~QSettingsWrapper();

    void clear();
    void sync();
    Status status() const;

    void beginGroup(const QString &prefix);
    void endGroup();
    QString group() const;

    int beginReadArray(const QString &prefix);
    void beginWriteArray(const QString &prefix, int size = -1);
    void endArray();
    void setArrayIndex(int i);

    QStringList allKeys() const;
    QStringList childKeys() const;
    QStringList childGroups() const;
    bool isWritable() const;

    void setValue(const QString &key, const QVariant &value);
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;

    void remove(const QString &key);
    bool contains(const QString &key) const;

    void setFallbacksEnabled(bool b);
    bool fallbacksEnabled() const;

    QString fileName() const;
    Format format() const;
    Scope scope() const;
    QString organizationName() const;
    QString applicationName() const;

private:
    bool createSocket() const;

private: // we cannot support the following functionality
    explicit QSettingsWrapper(QObject *parent = 0)
        : RemoteObject(QLatin1String(Protocol::QSettings), parent)
    {}

    void setIniCodec(QTextCodec * /*codec*/);
    void setIniCodec(const char * /*codecName*/);
    QTextCodec *iniCodec() const { return 0; }

    static void setDefaultFormat(Format /*format*/);
    static Format defaultFormat() { return NativeFormat; }
    static void setSystemIniPath(const QString & /*dir*/);
    static void setUserIniPath(const QString & /*dir*/);
    static void setPath(Format /*format*/, Scope /*scope*/, const QString & /*path*/);

    typedef QMap<QString, QVariant> SettingsMap;
    typedef bool(*ReadFunc)(QIODevice &device, SettingsMap &map);
    typedef bool(*WriteFunc)(QIODevice &device, const SettingsMap &map);

    static Format registerFormat(const QString &extension, ReadFunc readFunc, WriteFunc writeFunc,
        Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive)
    {
        Q_UNUSED(extension)
        Q_UNUSED(readFunc)
        Q_UNUSED(writeFunc)
        Q_UNUSED(caseSensitivity)
        return NativeFormat;
    }

private:
    class Private;
    Private *d;
};

} // namespace QInstaller

#endif  // QSETTINGSWRAPPER_H
