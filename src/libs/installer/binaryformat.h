/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef BINARYFORMAT_H
#define BINARYFORMAT_H

#include "range.h"
#include "qinstallerglobal.h"

#include <QFile>
#include <QVector>

namespace QInstallerCreator {

class INSTALLER_EXPORT Archive : public QIODevice
{
public:
    explicit Archive(const QString &path);
    Archive(const QByteArray &name, const QSharedPointer<QFile> &device, const Range<qint64> &segment);
    ~Archive();

    bool open(OpenMode mode);
    void close();

    bool seek(qint64 pos);
    qint64 size() const;

    QByteArray name() const;
    void setName(const QByteArray &name);

    void copyData(QFileDevice *out) { copyData(this, out); }
    static void copyData(Archive *archive, QFileDevice *out);

private:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    QSharedPointer<QFile> m_device;
    const Range<qint64> m_segment;
    QFile m_inputFile;
    QByteArray m_name;
};

class INSTALLER_EXPORT Component
{
    Q_DECLARE_TR_FUNCTIONS(Component)

public:
    static Component readFromIndexEntry(const QSharedPointer<QFile> &dev, qint64 offset);

    void writeIndexEntry(QFileDevice *dev, qint64 offset) const;

    void writeData(QFileDevice *dev, qint64 positionOffset) const;
    void readData(const QSharedPointer<QFile> &dev, qint64 offset);

    QByteArray name() const;
    void setName(const QByteArray &ba);

    void appendArchive(const QSharedPointer<Archive> &archive);
    QSharedPointer<Archive> archiveByName(const QByteArray &name) const;
    QVector< QSharedPointer<Archive> > archives() const;

    bool operator<(const Component &other) const;
    bool operator==(const Component &other) const;

private:
    QByteArray m_name;
    QVector<QSharedPointer<Archive> > m_archives;
    mutable Range<qint64> m_binarySegment;
    QString m_dataDirectory;
};


class INSTALLER_EXPORT ComponentIndex
{
public:
    ComponentIndex();
    static ComponentIndex read(const QSharedPointer<QFile> &dev, qint64 offset);
    void writeIndex(QFileDevice *dev, qint64 offset) const;
    void writeComponentData(QFileDevice *dev, qint64 offset) const;
    Component componentByName(const QByteArray &name) const;
    void insertComponent(const Component &name);
    void removeComponent(const QByteArray &name);
    QVector<Component> components() const;
    int componentCount() const;

private:
    QHash<QByteArray, Component> m_components;
};

} // namespace QInstallerCreator

#endif // BINARYFORMAT_H
