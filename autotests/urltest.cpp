/*
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * Copyright (c) 2019 David Barchiesi <david@barchie.si>
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
    const auto gdriveUrl = [](const QString &path) {
        QUrl url;
        url.setScheme(GDriveUrl::Scheme);
        url.setPath(path);
        return url;
    };

    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QString>("expectedToString");
    QTest::addColumn<QString>("expectedAccount");
    QTest::addColumn<QString>("expectedParentPath");
    QTest::addColumn<bool>("expectedIsTrashed");
    QTest::addColumn<bool>("expectedIsTopLevel");
    QTest::addColumn<QStringList>("expectedPathComponents");
    QTest::addColumn<QString>("expectedFilename");

    QTest::newRow("root url")
            << gdriveUrl(QStringLiteral("/"))
            << QStringLiteral("gdrive:/")
            << QString()
            << QString()
            << false
            << false
            << QStringList()
            << "";

    QTest::newRow("account root url")
            << gdriveUrl(QStringLiteral("/foo@gmail.com"))
            << QStringLiteral("gdrive:/foo@gmail.com")
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/")
            << false
            << false
            << QStringList {QStringLiteral("foo@gmail.com")}
            << QStringLiteral("foo@gmail.com");

    QTest::newRow("account trash url")
            << gdriveUrl(QStringLiteral("/foo@gmail.com/") + GDriveUrl::TrashDir)
            << QStringLiteral("gdrive:/foo@gmail.com/") + GDriveUrl::TrashDir
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << false
            << true
            << QStringList {QStringLiteral("foo@gmail.com"), GDriveUrl::TrashDir}
            << GDriveUrl::TrashDir;

    QTest::newRow("file in trash")
            << gdriveUrl(QStringLiteral("/foo@gmail.com/") + GDriveUrl::TrashDir + ("/baz.txt"))
            << QStringLiteral("gdrive:/foo@gmail.com/") + GDriveUrl::TrashDir + ("/baz.txt")
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com/") + GDriveUrl::TrashDir
            << true
            << false
            << QStringList {QStringLiteral("foo@gmail.com"), GDriveUrl::TrashDir, QStringLiteral("baz.txt")}
            << QStringLiteral("baz.txt");

    QTest::newRow("file in account root")
            << gdriveUrl(QStringLiteral("/foo@gmail.com/bar.txt"))
            << QStringLiteral("gdrive:/foo@gmail.com/bar.txt")
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << false
            << true
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar.txt")}
            << QStringLiteral("bar.txt");

    QTest::newRow("folder in account root - no trailing slash")
            << gdriveUrl(QStringLiteral("/foo@gmail.com/bar"))
            << QStringLiteral("gdrive:/foo@gmail.com/bar")
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << false
            << true
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar")}
            << QStringLiteral("bar");

    QTest::newRow("folder in account root - trailing slash")
            << gdriveUrl(QStringLiteral("/foo@gmail.com/bar/"))
            << QStringLiteral("gdrive:/foo@gmail.com/bar/")
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com")
            << false
            << true
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar")}
            << QStringLiteral("bar");

    QTest::newRow("file in subfolder")
            << gdriveUrl(QStringLiteral("/foo@gmail.com/bar/baz.txt"))
            << QStringLiteral("gdrive:/foo@gmail.com/bar/baz.txt")
            << QStringLiteral("foo@gmail.com")
            << QStringLiteral("/foo@gmail.com/bar")
            << false
            << false
            << QStringList {QStringLiteral("foo@gmail.com"), QStringLiteral("bar"), QStringLiteral("baz.txt")}
            << QStringLiteral("baz.txt");
}

void UrlTest::testGDriveUrl()
{
    QFETCH(QUrl, url);
    const auto gdriveUrl = GDriveUrl(url);

    QFETCH(QString, expectedToString);
    QCOMPARE(gdriveUrl.url(), QUrl(expectedToString));

    QFETCH(QString, expectedAccount);
    QFETCH(QString, expectedParentPath);
    QFETCH(bool, expectedIsTrashed);
    QFETCH(bool, expectedIsTopLevel);
    QFETCH(QStringList, expectedPathComponents);
    QFETCH(QString, expectedFilename);

    QCOMPARE(gdriveUrl.account(), expectedAccount);
    QCOMPARE(gdriveUrl.parentPath(), expectedParentPath);
    QCOMPARE(gdriveUrl.pathComponents(), expectedPathComponents);
    QCOMPARE(gdriveUrl.isTrashed(), expectedIsTrashed);
    QCOMPARE(gdriveUrl.isTopLevel(), expectedIsTopLevel);
    QCOMPARE(gdriveUrl.filename(), expectedFilename);

    if (expectedPathComponents.isEmpty()) {
        QVERIFY(gdriveUrl.isRoot());
    } else if (expectedPathComponents.count() == 1) {
        QVERIFY(gdriveUrl.isAccountRoot());
    }
}

#include "urltest.moc"
