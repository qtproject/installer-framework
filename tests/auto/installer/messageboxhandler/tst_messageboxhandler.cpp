/**************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include <messageboxhandler.h>
#include <qinstallerglobal.h>
#include <scriptengine.h>
#include <packagemanagercore.h>
#include <component.h>
#include <utils.h>
#include <fileutils.h>
#include <binarycontent.h>
#include <packagemanagercore.h>
#include <settings.h>
#include <init.h>

#include <QTest>
#include <QMetaEnum>
#include <QDebug>

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

using namespace QInstaller;

QT_BEGIN_NAMESPACE
namespace QTest {
    template<>
    char *toString(const QMessageBox::StandardButton &button)
    {
        QString buttonAsString(QString::number(button));
        return qstrdup(buttonAsString.toLatin1().data());
    }
}
QT_END_NAMESPACE

class tst_MessageBoxHandler : public QObject
{
    Q_OBJECT
private:
    void setRepository(const QString &repository) {
        core->cancelMetaInfoJob();
        QSet<Repository> repoList;
        Repository repo = Repository::fromUserInput(repository);
        repoList.insert(repo);
        core->settings().setDefaultRepositories(repoList);
    }

private slots:
    void initTestCase()
    {
        m_maxStandardButtons = 0;

        const QMetaObject &messageBoxMetaObject = QMessageBox::staticMetaObject;
        int index = messageBoxMetaObject.indexOfEnumerator("StandardButtons");

        QMetaEnum metaEnum = messageBoxMetaObject.enumerator(index);
        for (int i = 0; i < metaEnum.keyCount(); i++) {
            int enumValue = metaEnum.value(i);
            if (enumValue < QMessageBox::FirstButton)
                continue;
            m_standardButtonValueMap.insert(static_cast<QMessageBox::StandardButton>(enumValue),
                metaEnum.valueToKey(metaEnum.value(i)));
            m_maxStandardButtons += enumValue;
            if (enumValue == QMessageBox::LastButton)
                break;
        }

        QInstaller::init(); //This will eat debug output

        core = new PackageManagerCore(BinaryContent::MagicInstallerMarker, QList<OperationBlob> (),
                                      QString(), QString(), Protocol::DefaultAuthorizationKey, Protocol::Mode::Production,
                                      QHash<QString, QString>(), true);
        core->disableWriteMaintenanceTool();
        core->setAutoConfirmCommand();
        m_installDir = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(m_installDir));
        core->setValue(scTargetDir, m_installDir);
    }

    void testScriptButtonValues()
    {
        ScriptEngine *scriptEngine = new ScriptEngine(core);
        QMapIterator<QMessageBox::StandardButton, QString> i(m_standardButtonValueMap);
        while (i.hasNext()) {
            i.next();
            const QString scriptString = QString::fromLatin1("QMessageBox.%1").arg(i.value());
            const QJSValue scriptValue = scriptEngine->evaluate(scriptString);

            QVERIFY2(!scriptValue.isError(), qPrintable(scriptValue.toString()));
            QVERIFY2(!scriptValue.isUndefined(), qPrintable(
                QString::fromLatin1("It seems that %1 is undefined.").arg(scriptString)));
            QCOMPARE(static_cast<QMessageBox::StandardButton>(scriptValue.toInt()), i.key());
        }
    }

    void testDefaultAction()
    {
        srand(time(0)); /* initialize random seed: */

        int standardButtons = QMessageBox::NoButton;
        MessageBoxHandler::instance()->setDefaultAction(MessageBoxHandler::Reject);
        const QList<QMessageBox::Button> orderedButtons = MessageBoxHandler::orderedButtons();
        do {
            standardButtons += QMessageBox::FirstButton;

            /* generate secret number between 1 and 10: */
            const int iSecret = rand() % 10 + 1;
            // use only every 5th run to reduce the time which it takes to run this test
            if (iSecret > 2)
                continue;
            int returnButton = MessageBoxHandler::instance()->critical(QLatin1String("TestError"),
                QLatin1String("A test error"), QLatin1String("This is a test error message."),
                static_cast<QMessageBox::StandardButton>(standardButtons));

            QMessageBox::StandardButton wantedButton = QMessageBox::NoButton;
            // find the last button which is the wanted reject button in the current
            // standardButtons combination
            foreach (QMessageBox::StandardButton button, orderedButtons) {
                if (standardButtons & button)
                    wantedButton = button;
            }

            QVERIFY2(wantedButton != QMessageBox::NoButton, "Cannot find a wantedButton.");
            QCOMPARE(static_cast<QMessageBox::StandardButton>(returnButton), wantedButton);
        } while (standardButtons < m_maxStandardButtons);
    }

    void invalidOperationAutoReject()
    {
        setRepository(":///data/invalidoperation");
        core->autoRejectMessageBoxes();
        core->installDefaultComponentsSilently();
        QCOMPARE(PackageManagerCore::Canceled, core->status());
    }

    void invalidOperationAutoAccept()
    {
        setRepository(":///data/invalidoperation");
        core->autoAcceptMessageBoxes();
        core->installDefaultComponentsSilently();
        QCOMPARE(PackageManagerCore::Success, core->status());
    }

    void invalidHashAutoReject()
    {
        setRepository(":///data/invalidhash");
        core->autoRejectMessageBoxes();
        core->installSelectedComponentsSilently(QStringList () << "B");
        QCOMPARE(PackageManagerCore::Failure, core->status());
    }

    void invalidHashAutoAccept()
    {
        setRepository(":///data/invalidhash");
        core->autoAcceptMessageBoxes();
        core->installSelectedComponentsSilently(QStringList () << "B");
        QCOMPARE(PackageManagerCore::Failure, core->status());
    }

    void missingArchiveAutoReject()
    {
        setRepository(":///data/missingarchive");
        core->autoRejectMessageBoxes();
        core->installSelectedComponentsSilently(QStringList () << "C");
        QCOMPARE(PackageManagerCore::Canceled, core->status());
    }

    void missingArchiveAutoAccept()
    {
        setRepository(":///data/missingarchive");
        core->autoAcceptMessageBoxes();
        core->installSelectedComponentsSilently(QStringList () << "C");
        QCOMPARE(PackageManagerCore::Failure, core->status()); // Fails after retrying
    }

    void messageBoxFromScriptDefaultAnswer()
    {
        setRepository(":///data/messagebox");
        core->acceptMessageBoxDefaultButton();
        core->installSelectedComponentsSilently(QStringList () << "A");

        // These values are written in script based on default
        // messagebox query results.
        QCOMPARE(core->value("test.question.default.ok"), QLatin1String("Ok"));
        QCOMPARE(core->value("test.question.default.cancel"), QLatin1String("Cancel"));
    }

    void messageBoxFromScriptAutoAnswer()
    {
        setRepository(":///data/messagebox");
        core->setMessageBoxAutomaticAnswer("test.question.default.ok", QMessageBox::Cancel);
        core->setMessageBoxAutomaticAnswer("test.question.default.cancel", QMessageBox::Ok);
        core->installSelectedComponentsSilently(QStringList () << "A");

        // These values are written in script based on
        // messagebox query results.
        QCOMPARE(core->value("test.question.default.ok"), QLatin1String("Cancel"));
        QCOMPARE(core->value("test.question.default.cancel"), QLatin1String("Ok"));
    }

    void cleanupTestCase()
    {
        core->deleteLater();
        QDir dir(m_installDir);
        QVERIFY(dir.removeRecursively());
    }

private:
    QMap<QMessageBox::StandardButton, QString> m_standardButtonValueMap;
    int m_maxStandardButtons;
    PackageManagerCore *core;
    QString m_installDir;
};

QTEST_GUILESS_MAIN(tst_MessageBoxHandler)

#include "tst_messageboxhandler.moc"
