/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef BINARYFORMATENGINE_H
#define BINARYFORMATENGINE_H

#include "binaryformat.h"

#include <QtCore/private/qfsfileengine_p.h>

namespace QInstaller {

class BinaryFormatEngine : public QAbstractFileEngine
{
    Q_DISABLE_COPY(BinaryFormatEngine)

public:
    BinaryFormatEngine(const QHash<QByteArray, ResourceCollection> &collections,
        const QString &fileName);

    void setFileName(const QString &file);

    bool copy(const QString &newName);
    bool close();
    bool open(QIODevice::OpenMode mode);
    qint64 pos() const;
    qint64 read(char *data, qint64 maxlen);
    bool seek(qint64 offset);
    qint64 size() const;

    QString fileName(FileName file = DefaultName) const;
    FileFlags fileFlags(FileFlags type = FileInfoAll) const;

    Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames);
    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const;

private:
    QString m_fileNamePath;

    ResourceCollection m_collection;
    QSharedPointer<Resource> m_resource;

    QHash<QByteArray, ResourceCollection> m_collections;
};

} // namespace QInstaller

#endif // BINARYFORMATENGINE_H
