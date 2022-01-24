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

#include <genericfactory.h>

#include <QDebug>
#include <QTest>

#include <functional>

class Food {
public:
    explicit Food(int amount)
        : m_amount(amount)
    {}
    virtual ~Food() {}

    int available() const {
        return m_amount;
    }
    virtual QDate expireDate() const = 0;

    template <typename T, typename... Args>
    static Food *create(Args... args) {
        qDebug().nospace().noquote() << "Create function.";
        return new T(std::forward<Args>(args)...);
    }

private:
    int m_amount;
};

class EggStore : public GenericFactory<Food, QString, int>
{
public:
    static EggStore &instance() {
        static EggStore factory;
        return factory;
    }
};

class Bread : public Food
{
public:
    Bread(int amount, const QDate &expireDate)
        : Food(amount)
        , m_expireDate(expireDate)
    { qDebug().nospace().noquote() << "Constructor."; }
    QDate expireDate() const override {
        return m_expireDate;
    }
private:
    QDate m_expireDate;
};

class Butter : public Food
{
public:
    QDate expireDate() const override {
        return m_expireDate;
    }
    static Food *create(int amount, const QDate expireDate) {
        qDebug().nospace().noquote() << "Create function.";
        return new Butter(amount, expireDate);
    }

private:
    Butter(int amount, const QDate &expireDate)
        : Food(amount)
        , m_expireDate(expireDate)
    { qDebug().nospace().noquote() << "Constructor."; }

private:
    QDate m_expireDate;
};

class ColdCuts : public Food
{
public:
    ColdCuts(int amount, const QDate &expireDate)
        : Food(amount)
        , m_expireDate(expireDate)
    { qDebug().nospace().noquote() << "Constructor."; }
    QDate expireDate() const override {
        return m_expireDate;
    }

private:
    QDate m_expireDate;
};

class FoodStore : public GenericFactory<Food, QString, int, QDate>
{
public:
    static FoodStore &instance() {
        static FoodStore factory;
        return factory;
    }
};

class tst_Factory : public QObject
{
    Q_OBJECT

private slots:
    void testSingleArg()
    {
        class Egg : public Food
        {
        public:
            explicit Egg(int amount)
                : Food(amount)
            { qDebug().nospace().noquote() << "Constructor."; }
            QDate expireDate() const {
                return QDate::currentDate().addDays(1);
            }
        private:
            QDate m_expireDate;
        };
        class EggStore : public GenericFactory<Food, QString, int>
        {
        public:
            static EggStore &instance() {
                static EggStore factory;
                return factory;
            }
        };

        EggStore::instance().registerProduct<Egg>("Egg");
        QCOMPARE(EggStore::instance().containsProduct("Egg"), true);
        // EggStore::instance().registerProduct<Bread>("Bread"); // Does not compile.

        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        auto food = EggStore::instance().create("Egg", 100);
        QCOMPARE(food->available(), 100);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(1));

        QTest::ignoreMessage(QtDebugMsg, "Create function.");
        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        EggStore::instance().registerProduct("Egg", &Egg::create<Egg, int>);
        food = EggStore::instance().create("Egg", 10);
        QCOMPARE(food->available(), 10);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(1));

        auto lambda = [](int amount) -> Food* { return new Egg(amount); };
        EggStore::instance().registerProduct("Egg", lambda);

        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        food = EggStore::instance().create("Egg", 20);
        QCOMPARE(food->available(), 20);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(1));
    }

    void testMultiArg()
    {
        FoodStore::instance().registerProduct<Bread>("Bread");
        FoodStore::instance().registerProduct("Butter", &Butter::create);
        FoodStore::instance().registerProduct<ColdCuts>("ColdCuts");
        // FoodStore::instance().registerProduct<Eggs>("Eggs"); // Does not compile.

        QCOMPARE(FoodStore::instance().containsProduct("Bread"), true);
        QCOMPARE(FoodStore::instance().containsProduct("Butter"), true);
        QCOMPARE(FoodStore::instance().containsProduct("ColdCuts"), true);
        QCOMPARE(FoodStore::instance().containsProduct("Sandwich"), false);

        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        auto food = FoodStore::instance().create("Bread", 10, QDate::currentDate().addDays(7));
        QCOMPARE(food->available(), 10);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(7));

        QTest::ignoreMessage(QtDebugMsg, "Create function.");
        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        food = FoodStore::instance().create("Butter", 2, QDate::currentDate().addDays(3));
        QCOMPARE(food->available(), 2);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(3));

        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        food = FoodStore::instance().create("ColdCuts", 50, QDate::currentDate().addDays(5));
        QCOMPARE(food->available(), 50);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(5));

        food = FoodStore::instance().create("Sandwich", 50, QDate::currentDate());
        QCOMPARE(food == Q_NULLPTR, true);

        // overwrites the existing registration
        FoodStore::instance().registerProduct("ColdCuts", &ColdCuts::create<ColdCuts, int, QDate>);
        QTest::ignoreMessage(QtDebugMsg, "Create function.");
        QTest::ignoreMessage(QtDebugMsg, "Constructor.");
        food = FoodStore::instance().create("ColdCuts", 100, QDate::currentDate().addDays(4));
        QCOMPARE(food->available(), 100);
        QCOMPARE(food->expireDate(), QDate::currentDate().addDays(4));
    }
};

QTEST_MAIN(tst_Factory)

#include "tst_factory.moc"
