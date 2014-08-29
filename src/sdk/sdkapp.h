/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef SDKAPP_H
#define SDKAPP_H

#include <binarycontent.h>
#include <binaryformat.h>
#include <fileio.h>
#include <fileutils.h>

#include <QApplication>
#include <QBuffer>
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
        using namespace QInstaller;
        foreach (const QSharedPointer<Resource> &resource, resourceMappings.resources()) {
            resource->open();   // ignore error here, either we opened it or it is opened
            QResource::unregisterResource((const uchar *) resource->readAll().constData(),
                QLatin1String(":/metadata"));
        }
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
        Returns the installer/ maintenance tool binary. In case of an installer this will be the
        installer binary itself, which contains the binary layout and the binary content. In case
        of an maintenance tool, it will return a binary that has just a binary layout append.

        Note on OSX: For compatibility reason this function will return the a .dat file located
        inside the resource folder in the application bundle, as on OSX the binary layout can not
        be appended to the actual installer/ maintenance tool binary itself because of signing.
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
        Returns the corresponding .dat file for a given installer/ maintenance tool binary or an
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

    QInstaller::ResourceCollection registeredMetaResources()
    {
        return resourceMappings;
    }

    void registerMetaResources(const QInstaller::ResourceCollection &resources)
    {
        foreach (const QSharedPointer<QInstaller::Resource> &resource, resources.resources()) {
            const bool isOpen = resource->isOpen();
            if ((!isOpen) && (!resource->open()))
                continue;

            if (!resource->seek(0))
                continue;

            const QByteArray ba = resource->readAll();
            if (ba.isEmpty())
                continue;

            if (QResource::registerResource((const uchar*) ba.data(), QLatin1String(":/metadata"))) {
                using namespace QInstaller;
                QSharedPointer<QBuffer> buffer(new QBuffer);
                buffer->setData(ba); // set the buffers internal data
                resourceMappings.appendResource(QSharedPointer<Resource>(new Resource(buffer)));
            }

        if (!isOpen) // If we reach that point, either the resource was opened already...
            resource->close();           // or we did open it and have to close it again.
        }
    }

private:
    QInstaller::ResourceCollection resourceMappings;
};

#endif  // SDKAPP_H
