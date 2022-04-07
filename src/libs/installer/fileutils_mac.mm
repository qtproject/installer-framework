/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "fileutils.h"

#include "errors.h"

#include <QCoreApplication>

#include <objc/objc.h>
#include <Foundation/NSURL.h>
#include <Foundation/NSError.h>>

namespace QInstaller {

/*!
    \internal

    Creates a bookmark variant of Finder alias from target \a path to \a alias.
    Throws \c Error on failure.
*/
void mkalias(const QString &path, const QString &alias)
{
    NSURL *targetUrl = [NSURL fileURLWithPath:path.toNSString()];
    NSURL *aliasUrl = [NSURL fileURLWithPath:alias.toNSString()];

    NSError *error = nil;
    NSData *data = [targetUrl bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
        includingResourceValuesForKeys:nil relativeToURL:nil error:&error];

    if (data != nil) {
        BOOL success = [NSURL writeBookmarkData:data toURL:aliasUrl
            options:NSURLBookmarkCreationSuitableForBookmarkFile error:&error];
        if (success == NO) {
            throw Error(QCoreApplication::translate("QInstaller",
                "Cannot create alias from \"%1\" to \"%2\": %3.")
                    .arg(path, alias, QString::fromNSString(error.localizedDescription))
            );
        }
    } else {
        throw Error(QCoreApplication::translate("QInstaller",
            "Could not get bookmark from URL \"%1\": %2.").arg(
                QString::fromNSString(targetUrl.absoluteString),
                QString::fromNSString(error.localizedDescription)
            )
        );
    }
}

} // namespace QInstaller
