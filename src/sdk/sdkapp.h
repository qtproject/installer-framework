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

#ifndef SDKAPP_H
#define SDKAPP_H

#include <binarycontent.h>
#include <binaryformat.h>
#include <fileio.h>
#include <fileutils.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QResource>

template<class T>
class SDKApp : public T
{
public:
    SDKApp(int& argc, char** argv)
        : T(argc, argv)
    {
    }

    virtual ~SDKApp()
    {
        foreach (const QByteArray &ba, m_resourceMappings)
            QResource::unregisterResource((const uchar*) ba.data(), QLatin1String(":/metadata"));
    }

    bool notify(QObject *receiver, QEvent *event)
    {
        try {
            return T::notify(receiver, event);
        } catch (std::exception &e) {
            qFatal("Exception thrown: %s", e.what());
        } catch (...) {
            qFatal("Unknown exception caught.");
        }
        return false;
    }

    /*!
        Returns the installer / maintenance tool binary. In case of an installer this will be the
        installer binary itself, which contains the binary layout and the binary content. In case
        of an maintenance tool, it will return a binary that has just a binary layout append.

        Note on OS X: For compatibility reason this function will return the a .dat file located
        inside the resource folder in the application bundle, as on OS X the binary layout cannot
        be appended to the actual installer / maintenance tool binary itself because of signing.
    */
    QString binaryFile() const
    {
        QString binaryFile = QCoreApplication::applicationFilePath();
#ifdef Q_OS_OSX
        // The installer binary on OSX does not contain the binary content, it's put into
        // the resources folder as separate file. Adjust the actual binary path. No error
        // checking here since we will fail later while reading the binary content.
        QDir resourcePath(QFileInfo(binaryFile).dir());
        resourcePath.cdUp();
        resourcePath.cd(QLatin1String("Resources"));
        return resourcePath.filePath(QLatin1String("installer.dat"));
#endif
        return binaryFile;
    }

    /*!
        Returns the corresponding .dat file for a given installer / maintenance tool binary or an
        empty string if it fails to find one.
    */
    QString datFile(const QString &binaryFile) const
    {
        QFile file(binaryFile);
        QInstaller::openForRead(&file);
        const quint64 cookiePos = QInstaller::BinaryContent::findMagicCookie(&file,
            QInstaller::BinaryContent::MagicCookie);
        if (!file.seek(cookiePos - sizeof(qint64)))    // seek to read the marker
            return QString(); // ignore error, we will fail later

        const qint64 magicMarker = QInstaller::retrieveInt64(&file);
        if (magicMarker == QInstaller::BinaryContent::MagicUninstallerMarker) {
            QFileInfo fi(binaryFile);
            QString bundlePath;
            if (QInstaller::isInBundle(fi.absoluteFilePath(), &bundlePath))
                fi.setFile(bundlePath);
            return fi.absoluteDir().filePath(fi.baseName() + QLatin1String(".dat"));
        }
        return QString();
    }

    void registerMetaResources(const QInstaller::ResourceCollection &collection)
    {
        foreach (const QSharedPointer<QInstaller::Resource> &resource, collection.resources()) {
            const bool isOpen = resource->isOpen();
            if ((!isOpen) && (!resource->open()))
                continue;

            if (!resource->seek(0))
                continue;

            const QByteArray ba = resource->readAll();
            if (ba.isEmpty())
                continue;

            if (QResource::registerResource((const uchar*) ba.data(), QLatin1String(":/metadata")))
                m_resourceMappings.append(ba);

            if (!isOpen) // If we reach that point, either the resource was opened already...
                resource->close();           // or we did open it and have to close it again.
        }
    }

private:
    QList<QByteArray> m_resourceMappings;
};

#endif  // SDKAPP_H
