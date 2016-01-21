#include <messageboxhandler.h>
#include <qinstallerglobal.h>
#include <scriptengine.h>
#include <packagemanagercore.h>

#include <QTest>
#include <QMetaEnum>
#include <QDebug>

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

using namespace QInstaller;

class tst_MessageBoxHandler : public QObject
{
    Q_OBJECT
public:
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
    }

    void testScriptButtonValues()
    {
        PackageManagerCore core;
        ScriptEngine *scriptEngine = new ScriptEngine(&core);
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
        const char ignoreMessage[] = "\"created critical message box TestError: 'A test error', "
            "This is a test error message.\" ";
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

            QTest::ignoreMessage(QtDebugMsg, ignoreMessage);
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

            QVERIFY2(wantedButton != QMessageBox::NoButton, "Could not find a wantedButton.");
            QCOMPARE(static_cast<QMessageBox::StandardButton>(returnButton), wantedButton);
        } while (standardButtons < m_maxStandardButtons);
    }

private:
    QMap<QMessageBox::StandardButton, QString> m_standardButtonValueMap;
    int m_maxStandardButtons;
};

QTEST_MAIN(tst_MessageBoxHandler)

#include "tst_messageboxhandler.moc"
