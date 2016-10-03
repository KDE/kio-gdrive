/*
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "../src/gdriveurl.h"

#include <QTest>

class UrlTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPathComponents_data();
    void testPathComponents();
};

QTEST_GUILESS_MAIN(UrlTest)

void UrlTest::testPathComponents_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QStringList>("expectedPathComponents");

    QTest::newRow("root url") << QUrl(QStringLiteral("gdrive://")) << QStringList();
    QTest::newRow("account root url") << QUrl(QStringLiteral("gdrive://foo@gmail.com")) << QStringList();
    QTest::newRow("file in account root") << QUrl(QStringLiteral("gdrive://foo@gmail.com/bar.txt"))
                                 << QStringList {QStringLiteral("bar.txt")};
    QTest::newRow("folder in account root - no trailing slash") << QUrl(QStringLiteral("gdrive://foo@gmail.com/bar"))
                                 << QStringList {QStringLiteral("bar")};
    QTest::newRow("folder in account root - trailing slash") << QUrl(QStringLiteral("gdrive://foo@gmail.com/bar/"))
                                 << QStringList {QStringLiteral("bar")};
    QTest::newRow("wrong url (slash before host/authority)") << QUrl(QStringLiteral("gdrive:///foo@gmail.com/bar.txt"))
                                 << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar.txt")};
    QTest::newRow("file in subfolder") << QUrl(QStringLiteral("gdrive://foo@gmail.com/bar/baz.txt"))
                                 << QStringList {QStringLiteral("bar"), QStringLiteral("baz.txt")};
}

void UrlTest::testPathComponents()
{
    QFETCH(QUrl, url);
    QFETCH(QStringList, expectedPathComponents);

    const auto gdriveUrl = GDriveUrl(url);

    QCOMPARE(gdriveUrl.pathComponents(), expectedPathComponents);

    if (expectedPathComponents.isEmpty()) {
        QVERIFY(gdriveUrl.isRoot());
    } else if (expectedPathComponents.count() == 1) {
        QVERIFY(gdriveUrl.isAccountRoot());
    }
}

#include "urltest.moc"
