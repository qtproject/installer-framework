/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include <fsengineclient.h>

#include <QProcess>
#include <QApplication>
#include <QThread>
#include <QDebug>

class OutputHandler : public QObject
{
    Q_OBJECT

public:
    OutputHandler(QProcess* processWrapper, QObject *parent = 0)
        : QObject (parent)
    {
        m_process = processWrapper;
        QObject::connect(m_process, SIGNAL(readyRead()),
            this, SLOT(readProcessOutput()), Qt::DirectConnection );
    }
    void clearSavedOutPut() { m_savedOutPut.clear(); }
    QByteArray savedOutPut() { return m_savedOutPut; }

public slots:
       void readProcessOutput()
       {
           Q_ASSERT(m_process);
           Q_ASSERT(QThread::currentThread() == m_process->thread());
           if (QThread::currentThread() != m_process->thread()) {
               qDebug() << Q_FUNC_INFO << QLatin1String(" can only be called from the same thread as the process is.") ;
           }
           const QByteArray output = m_process->readAll();
           if( !output.isEmpty() ) {
               m_savedOutPut.append(output);
               qDebug() << output;
           }
       }

private:
        QProcess* m_process;
        QByteArray m_savedOutPut;
};


int main(int argc, char **argv)
{
    QApplication app(argc, argv);


    QString fileName(QLatin1String("give_me_some_output.bat"));
    QFile file(fileName);
    if (file.exists() && !file.remove()) {
        qFatal( qPrintable( QString("something is wrong, can not delete: %1").arg(file.fileName()) ) );
        return -1;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qFatal( qPrintable( QString("something is wrong, can not open for writing to: %1").arg(file.fileName()) ) );
        return -2;
    }

    QTextStream out(&file);
    out << QLatin1String("echo mega test output");
    if (!file.flush())
    {
        qFatal( qPrintable( QString("something is wrong, can not write to: %1").arg(file.fileName()) ) );
        return -3;
    }
    file.close();


//first run as a normal QProcess
    QByteArray firstOutPut;
    {
        QProcess process;
        OutputHandler outputer(&process);

        process.start(fileName);
        qDebug() << "1";
        {
            const QByteArray output = process.readAll();
            if( !output.isEmpty() ) {
                qDebug() << output;
            }
        }
        process.waitForStarted();
        qDebug() << "2";
        {
            const QByteArray output = process.readAll();
            if( !output.isEmpty() ) {
                qDebug() << output;
            }
        }
        process.waitForFinished();
        qDebug() << "3";
        {
            const QByteArray output = process.readAll();
            if( !output.isEmpty() ) {
                qDebug() << output;
            }
        }
        firstOutPut = outputer.savedOutPut();
    }
//first run as a normal QProcess
    QByteArray secondOutPut;
    {
        FSEngineClientHandler::instance().enableTestMode();
        FSEngineClientHandler::instance().init(39999);
        FSEngineClientHandler::instance().setActive(true);

        QProcess process;
        OutputHandler outputer(&process);

        process.start(fileName);
        qDebug() << "1";
        {
            const QByteArray output = process.readAll();
            if( !output.isEmpty() ) {
                qDebug() << output;
            }
        }
        process.waitForStarted();
        qDebug() << "2";
        {
            const QByteArray output = process.readAll();
            if( !output.isEmpty() ) {
                qDebug() << output;
            }
        }
        process.waitForFinished();
        qDebug() << "3";
        {
            const QByteArray output = process.readAll();
            if( !output.isEmpty() ) {
                qDebug() << output;
            }
        }
        secondOutPut = outputer.savedOutPut();
    }

    if (firstOutPut != secondOutPut) {
        qFatal( qPrintable( QString("Test failed: output is different between a normal QProcess and QProcessWrapper").arg(file.fileName()) ) );
        return -2;
    } else {
        qDebug() << QLatin1String("Test OK: QProcess works as expected.");
    }

    return app.exec();
}

#include "fileengineclient.moc"
