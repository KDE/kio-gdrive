/*
 * Copyright (C) 2014  Daniel Vrátil <dvratil@redhat.com>
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

#include "accountmanager.h"

#include <KGAPI/AuthJob>
#include <QEventLoop>

//for stat.h
#include <KIO/Job>


QString AccountManager::s_apiKey = QStringLiteral("554041944266.apps.googleusercontent.com");
QString AccountManager::s_apiSecret = QStringLiteral("mdT1DjzohxN3npUUzkENT0gO");


AccountManager::AccountManager()
{
    m_wallet = KWallet::Wallet::openWallet(
        KWallet::Wallet::NetworkWallet(), 0,
        KWallet::Wallet::Synchronous);
    if (!m_wallet) {
        qCWarning(LOG_KIO_GDRIVE) << "Failed to open KWallet";
    } else {
        if (!m_wallet->hasFolder(QLatin1String("GDrive"))) {
            m_wallet->createFolder(QLatin1String("GDrive"));
        }

        m_wallet->setFolder(QLatin1String("GDrive"));
    }
}

AccountManager::~AccountManager()
{
    if (m_wallet) {
        m_wallet->closeWallet(KWallet::Wallet::NetworkWallet(), false);
    }
}

QStringList AccountManager::accounts()
{
    if (!m_wallet) {
        return QStringList();
    }

    return m_wallet->entryList();
}

KGAPI2::AccountPtr AccountManager::account(const QString &accountName)
{
    if (!m_wallet) {
        return KGAPI2::AccountPtr();
    }

    KGAPI2::AccountPtr account;
    if (accountName.isEmpty() || !m_wallet->entryList().contains(accountName)) {
        account = KGAPI2::AccountPtr(new KGAPI2::Account(accountName));
        account->addScope(QUrl("https://www.googleapis.com/auth/drive"));
        account->addScope(QUrl("https://www.googleapis.com/auth/drive.file"));
        account->addScope(QUrl("https://www.googleapis.com/auth/drive.metadata.readonly"));
        account->addScope(QUrl("https://www.googleapis.com/auth/drive.readonly"));

        KGAPI2::AuthJob *authJob = new KGAPI2::AuthJob(account, s_apiKey, s_apiSecret);

        QEventLoop eventLoop;
        QObject::connect(authJob, SIGNAL(finished(KGAPI2::Job*)),
                          &eventLoop, SLOT(quit()));
        eventLoop.exec();

        account = authJob->account();
        authJob->deleteLater();

        storeAccount(account);
    } else {
        QMap<QString, QString> entry;
        m_wallet->readMap(accountName, entry);

        const QStringList scopes = entry.value(QStringLiteral("scopes")).split(QLatin1Char(','), QString::SkipEmptyParts);
        QList<QUrl> scopeUrls;
        Q_FOREACH (const QString &scope, scopes) {
            scopeUrls << QUrl::fromUserInput(scope);
        }

        account = KGAPI2::AccountPtr(new KGAPI2::Account(accountName,
                                                         entry.value(QStringLiteral("accessToken")),
                                                         entry.value(QStringLiteral("refreshToken")),
                                                         scopeUrls));
    }

    return account;
}

void AccountManager::storeAccount(const KGAPI2::AccountPtr &account)
{
    if (!m_wallet) {
        return;
    }

    qCDebug(LOG_KIO_GDRIVE) << "Storing account" << account->accessToken();

    QMap<QString, QString> entry;
    entry[ QStringLiteral("accessToken") ] = account->accessToken();
    entry[ QStringLiteral("refreshToken") ] = account->refreshToken();
    QStringList scopes;
    Q_FOREACH (const QUrl &scope, account->scopes()) {
        scopes << scope.toString();
    }
    entry[ QStringLiteral("scopes") ] = scopes.join(QLatin1Char(','));

    m_wallet->writeMap(account->accountName(), entry);
}

KGAPI2::AccountPtr AccountManager::refreshAccount(const KGAPI2::AccountPtr &account)
{
    KGAPI2::AuthJob *authJob = new KGAPI2::AuthJob(account, s_apiKey, s_apiSecret);
    QEventLoop eventLoop;
    QObject::connect(authJob, SIGNAL(finished(KGAPI2::Job*)),
                      &eventLoop, SLOT(quit()));
    eventLoop.exec();
    if (authJob->error() != KGAPI2::OK && authJob->error() != KGAPI2::NoError) {
        return KGAPI2::AccountPtr();
    }

    const KGAPI2::AccountPtr newAccount = authJob->account();
    storeAccount(newAccount);
    return newAccount;
}

KIO::UDSEntry AccountManager::accountToUDSEntry(const QString &accountNAme)
{
    KIO::UDSEntry entry;

    entry.insert(KIO::UDSEntry::UDS_NAME, accountNAme);
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, accountNAme);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    entry.insert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("im-google"));

    return entry;
}

void AccountManager::removeAccount(const QString &accountName)
{
    if (!m_wallet) {
        return;
    }

    m_wallet->removeEntry(accountName);
}
