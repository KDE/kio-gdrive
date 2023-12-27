/*
 * SPDX-FileCopyrightText: 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

    connect(listJob, &KIO::ListJob::entries, this, [=](KIO::Job *, const KIO::UDSEntryList &list) {
        m_entries = list.count();
        for (const auto &entry : list) {
            // Check properties of new-account entry.
            if (entry.stringValue(KIO::UDSEntry::UDS_NAME) == QLatin1String("new-account")) {
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
