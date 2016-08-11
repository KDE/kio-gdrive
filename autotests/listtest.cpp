/*
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kdemail.net>
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

#include <QTest>

#include <KIO/ListJob>

class ListTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testListJob();

private:
    int m_entries = 0;
};

QTEST_GUILESS_MAIN(ListTest)

void ListTest::testListJob()
{
    const auto gdriveRoot = QUrl(QStringLiteral("gdrive:/"));
    QVERIFY(gdriveRoot.isValid());

    auto listJob = KIO::listDir(gdriveRoot, KIO::HideProgressInfo);
    listJob->setUiDelegate(nullptr);
    QVERIFY(listJob);

    connect(listJob, &KIO::ListJob::entries, this, [=](KIO::Job*, const KIO::UDSEntryList& list) {
        m_entries = list.count();
        foreach (const auto &entry, list) {
            // Check properties of new-account entry.
            if (entry.stringValue(KIO::UDSEntry::UDS_NAME) == QStringLiteral("new-account")) {
                QVERIFY(entry.isDir());
                QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME), QStringLiteral("list-add-user"));
            }
        }
    });

    connect(listJob, &KJob::result, this, [=](KJob *job) {
        QVERIFY(!job->error());
        // At least new-account entry.
        QVERIFY(m_entries > 0);
    });

    QEventLoop eventLoop;
    connect(listJob, &KJob::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
}

#include "listtest.moc"
