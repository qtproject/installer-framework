#include "messageboxhandler.h"
#include "qinstallerglobal.h"

#include <QTest>
#include <QMetaEnum>
#include <QScriptEngine>
#include <QDebug>

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

using namespace QInstaller;

namespace QTest {
    template<>
    char *toString(const QMessageBox::StandardButton &button)
    {
        QString buttonAsString(QString::number(button));
        return qstrdup(buttonAsString.toLatin1().data());
    }
}

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
        QInstaller::registerMessageBox(&m_scriptEngine);
    }

    void testScriptButtonValues()
    {
        QMapIterator<QMessageBox::StandardButton, QString> i(m_standardButtonValueMap);
        while (i.hasNext()) {
            i.next();
            QString scriptString = QString::fromLatin1("QMessageBox.%1").arg(i.value());
            QScriptValue scriptValue(m_scriptEngine.evaluate(scriptString));

            QVERIFY2(!scriptValue.isUndefined(), qPrintable(
                QString::fromLatin1("It seems that %1 is undefined.").arg(scriptString)));

            qint32 evaluatedValue = scriptValue.toInt32();
            QVERIFY2(!m_scriptEngine.hasUncaughtException(), qPrintable(
                QInstaller::uncaughtExceptionString(&m_scriptEngine)));

            QCOMPARE(static_cast<QMessageBox::StandardButton>(evaluatedValue), i.key());
        }
    }

    void testDefaultAction()
    {
        int standardButtons = QMessageBox::NoButton;
        QList<QMessageBox::Button> orderedButtons = MessageBoxHandler::orderedButtons();
        MessageBoxHandler *messageBoxHandler = MessageBoxHandler::instance();

        messageBoxHandler->setDefaultAction(MessageBoxHandler::Reject);
        QString testidentifier(QLatin1String("TestError"));
        QString testTitle(QLatin1String("A test error"));
        QString testMessage(QLatin1String("This is a test error message."));

        const char *ignoreMessage("\"created critical message box TestError: 'A test error', This is a test error message.\" ");
        /* initialize random seed: */
        srand(time(0));
        do {
            standardButtons += QMessageBox::FirstButton;

            /* generate secret number between 1 and 10: */
            int iSecret = rand() % 10 + 1;
            // use only every 5th run to reduce the time which it takes to run this test
            if (iSecret > 2)
                continue;
            QTest::ignoreMessage(QtDebugMsg, ignoreMessage);
            const QMessageBox::StandardButton returnButton = static_cast<QMessageBox::StandardButton>(
                messageBoxHandler->critical(testidentifier, testTitle, testMessage,
                static_cast<QMessageBox::StandardButton>(standardButtons)));

            QMessageBox::StandardButton wantedButton = QMessageBox::NoButton;
            // find the last button which is the wanted reject button in the current
            // standardButtons combination
            foreach (QMessageBox::StandardButton button, orderedButtons) {
                if (standardButtons & button)
                    wantedButton = button;
            }

            QVERIFY2(wantedButton != QMessageBox::NoButton, "Could not find a wantedButton.");
            QCOMPARE(returnButton, wantedButton);

        } while (standardButtons < m_maxStandardButtons);
    }

private:
    QMap<QMessageBox::StandardButton, QString> m_standardButtonValueMap;
    int m_maxStandardButtons;
    QScriptEngine m_scriptEngine;
};

QTEST_MAIN(tst_MessageBoxHandler)

#include "tst_messageboxhandler.moc"
