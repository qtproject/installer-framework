/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef BINARYFORMATENGINE_H
#define BINARYFORMATENGINE_H

#include <QAbstractFileEngine>

#include "binaryformat.h"

namespace QInstallerCreator {

class BinaryFormatEngine : public QAbstractFileEngine
{
public:
    BinaryFormatEngine(const ComponentIndex &index, const QString &fileName);
    ~BinaryFormatEngine();

    void setFileName(const QString &file);

    Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames);

    bool copy(const QString &newName);
    bool close();
    bool open(QIODevice::OpenMode mode);
    qint64 pos() const;
    qint64 read(char *data, qint64 maxlen);
    bool seek(qint64 offset);
    qint64 size() const;

    QString fileName(FileName file = DefaultName) const;
    FileFlags fileFlags(FileFlags type = FileInfoAll) const;
    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const;

protected:
    void setArchive(const QString &file);

private:
    const ComponentIndex m_index;
    bool m_hasComponent;
    bool m_hasArchive;
    Component m_component;
    QSharedPointer<Archive> m_archive;
    QString m_fileNamePath;
};

} // namespace QInstallerCreator

#endif
