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

#include "simplemovefileoperation.h"

#include <QDir>
#include <QtCore/QFileInfo>

namespace QInstaller {

SimpleMoveFileOperation::SimpleMoveFileOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("SimpleMoveFile"));
}

void SimpleMoveFileOperation::backup()
{
}

bool SimpleMoveFileOperation::performOperation()
{
    if (!checkArgumentCount(2))
        return false;

    const QStringList args = arguments();
    const QString source = args.at(0);
    const QString target = args.at(1);

    if (source.isEmpty() || target.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("None of the arguments can be empty: source \"%1\", target \"%2\".")
            .arg(QDir::toNativeSeparators(source), QDir::toNativeSeparators(target)));
        return false;
    }

    // If destination file exists, then we cannot use QFile::copy() because it does not overwrite an existing
    // file. So we remove the destination file.
    QFile file(target);
    if (file.exists()) {
        if (!file.remove()) {
            setError(UserDefinedError);
            setErrorString(tr("Cannot move file from \"%1\" to \"%2\", because the target path exists and is "
                "not removable.").arg(QDir::toNativeSeparators(source), QDir::toNativeSeparators(target)));
            return false;
        }
    }

    file.setFileName(source);
    if (!file.rename(target)) {
        setError(UserDefinedError);
        setErrorString(tr("Cannot move file \"%1\" to \"%2\": %3").arg(
                           QDir::toNativeSeparators(source), QDir::toNativeSeparators(target),
                           file.errorString()));
        return false;
    }

    emit outputTextChanged(tr("Moving file \"%1\" to \"%2\".").arg(QDir::toNativeSeparators(source),
                                                                   QDir::toNativeSeparators(target)));
    return true;
}

bool SimpleMoveFileOperation::undoOperation()
{
    const QString source = arguments().at(0);
    const QString target = arguments().at(1);

    QFile(target).rename(source);
    emit outputTextChanged(tr("Moving file \"%1\" to \"%2\".").arg(QDir::toNativeSeparators(target),
                                                                   QDir::toNativeSeparators(source)));

    return true;
}

bool SimpleMoveFileOperation::testOperation()
{
    return true;
}

}   // namespace QInstaller
