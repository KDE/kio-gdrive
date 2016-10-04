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
    void testGDriveUrl_data();
    void testGDriveUrl();
};

QTEST_GUILESS_MAIN(UrlTest)

void UrlTest::testGDriveUrl_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QString>("expectedAccount");
    QTest::addColumn<QString>("expectedParentPath");
    QTest::addColumn<QStringList>("expectedPathComponents");

    QTest::newRow("root url")
            << QUrl(QStringLiteral("gdrive://"))
            << QString()
            << QString()
            << QStringList();

    QTest::newRow("account root url")
            << QUrl(QStringLiteral("gdrive:///foo@gmail.com"))
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/")
            << QStringList {QStringLiteral("foo@gmail.com")};

    QTest::newRow("file in account root")
            << QUrl(QStringLiteral("gdrive:///foo@gmail.com/bar.txt"))
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar.txt")};

    QTest::newRow("folder in account root - no trailing slash")
            << QUrl(QStringLiteral("gdrive:///foo@gmail.com/bar"))
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar")};
    QTest::newRow("folder in account root - trailing slash")
            << QUrl(QStringLiteral("gdrive:///foo@gmail.com/bar/"))
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar")};

    QTest::newRow("file in subfolder")
            << QUrl(QStringLiteral("gdrive:///foo@gmail.com/bar/baz.txt"))
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com/bar")
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar"), QStringLiteral("baz.txt")};
}

void UrlTest::testGDriveUrl()
{
    QFETCH(QUrl, url);
    QFETCH(QString, expectedAccount);
    QFETCH(QString, expectedParentPath);
    QFETCH(QStringList, expectedPathComponents);

    const auto gdriveUrl = GDriveUrl(url);

    QCOMPARE(gdriveUrl.account(), expectedAccount);
    QCOMPARE(gdriveUrl.parentPath(), expectedParentPath);
    QCOMPARE(gdriveUrl.pathComponents(), expectedPathComponents);

    if (expectedPathComponents.isEmpty()) {
        QVERIFY(gdriveUrl.isRoot());
    } else if (expectedPathComponents.count() == 1) {
        QVERIFY(gdriveUrl.isAccountRoot());
    }
}

#include "urltest.moc"
