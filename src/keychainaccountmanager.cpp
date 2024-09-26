/*
 * SPDX-FileCopyrightText: 2014 Daniel Vrátil <dvratil@redhat.com>
 * SPDX-FileCopyrightText: 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "keychainaccountmanager.h"
#include "gdrivedebug.h"

#include <QDataStream>
#include <QEventLoop>
#include <QIODevice>

#include <qt6keychain/keychain.h>

#include <KGAPI/AuthJob>
#include <KIO/Job> //for stat.h

QString KeychainAccountManager::s_apiKey = QStringLiteral("836279663462-qq9rt1k4smgqhvt461r6l54vo9qm09bg.apps.googleusercontent.com");
QString KeychainAccountManager::s_apiSecret = QStringLiteral("5-k19mrwsAud5a1r-qijtTJQ");

QSet<QString> KeychainAccountManager::accounts()
{
    if (m_accounts.isEmpty()) {
        auto job = new QKeychain::ReadPasswordJob(QStringLiteral("KIO GDrive"));
        job->setKey(QStringLiteral("gdrive-accounts"));
        runKeychainJob(job);

        auto data = job->binaryData();
        m_accounts = deserialize<QSet<QString>>(&data);

        qCDebug(GDRIVE) << "Fetched" << m_accounts.count() << "account(s) from keychain";
    }

    return m_accounts;
}

KGAPI2::AccountPtr KeychainAccountManager::account(const QString &accountName)
{
    if (!accounts().contains(accountName)) {
        return KGAPI2::AccountPtr(new KGAPI2::Account());
    }

    const auto entry = readMap(accountName);

    const QStringList scopes = entry.value(QStringLiteral("scopes")).split(QLatin1Char(','), Qt::SkipEmptyParts);
    QList<QUrl> scopeUrls;
    for (const QString &scope : scopes) {
        scopeUrls << QUrl::fromUserInput(scope);
    }

    return KGAPI2::AccountPtr(
        new KGAPI2::Account(accountName, entry.value(QStringLiteral("accessToken")), entry.value(QStringLiteral("refreshToken")), scopeUrls));
}

KGAPI2::AccountPtr KeychainAccountManager::createAccount()
{
    auto account = KGAPI2::AccountPtr(new KGAPI2::Account());
    account->addScope(QUrl(QStringLiteral("https://www.googleapis.com/auth/drive")));
    account->addScope(QUrl(QStringLiteral("https://www.googleapis.com/auth/drive.file")));
    account->addScope(QUrl(QStringLiteral("https://www.googleapis.com/auth/drive.metadata.readonly")));
    account->addScope(QUrl(QStringLiteral("https://www.googleapis.com/auth/drive.readonly")));

    KGAPI2::AuthJob *authJob = new KGAPI2::AuthJob(account, s_apiKey, s_apiSecret);

    QEventLoop eventLoop;
    QObject::connect(authJob, &KGAPI2::Job::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();

    account = authJob->account();
    authJob->deleteLater();

    if (!account->accountName().isEmpty()) {
        storeAccount(account);
    }

    return account;
}

void KeychainAccountManager::storeAccount(const KGAPI2::AccountPtr &account)
{
    qCDebug(GDRIVE) << "Storing account" << account->accessToken();

    QMap<QString, QString> entry;
    entry[QStringLiteral("accessToken")] = account->accessToken();
    entry[QStringLiteral("refreshToken")] = account->refreshToken();
    QStringList scopes;
    const auto accountScopes = account->scopes();
    for (const QUrl &scope : accountScopes) {
        scopes << scope.toString();
    }
    entry[QStringLiteral("scopes")] = scopes.join(QLatin1Char(','));

    writeMap(account->accountName(), entry);
    storeAccountName(account->accountName());
}

KGAPI2::AccountPtr KeychainAccountManager::refreshAccount(const KGAPI2::AccountPtr &account)
{
    KGAPI2::AuthJob *authJob = new KGAPI2::AuthJob(account, s_apiKey, s_apiSecret);
    QEventLoop eventLoop;
    QObject::connect(authJob, &KGAPI2::Job::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    if (authJob->error() != KGAPI2::OK && authJob->error() != KGAPI2::NoError) {
        return KGAPI2::AccountPtr();
    }

    const KGAPI2::AccountPtr newAccount = authJob->account();
    storeAccount(newAccount);
    return newAccount;
}

void KeychainAccountManager::removeAccountName(const QString &accountName)
{
    auto accounts = this->accounts();
    accounts.remove(accountName);

    const auto data = serialize<QSet<QString>>(accounts);

    auto job = new QKeychain::WritePasswordJob(QStringLiteral("KIO GDrive"));
    job->setKey(QStringLiteral("gdrive-accounts"));
    job->setBinaryData(data);
    runKeychainJob(job);

    m_accounts = accounts;
}

void KeychainAccountManager::storeAccountName(const QString &accountName)
{
    auto accounts = this->accounts();
    accounts.insert(accountName);

    const auto data = serialize<QSet<QString>>(accounts);

    auto job = new QKeychain::WritePasswordJob(QStringLiteral("KIO GDrive"));
    job->setKey(QStringLiteral("gdrive-accounts"));
    job->setBinaryData(data);
    runKeychainJob(job);

    m_accounts = accounts;
}

QMap<QString, QString> KeychainAccountManager::readMap(const QString &accountName)
{
    auto job = new QKeychain::ReadPasswordJob(QStringLiteral("KIO GDrive"));
    job->setKey(accountName);
    runKeychainJob(job);

    if (job->error()) {
        return {};
    }

    auto data = job->binaryData();
    return deserialize<QMap<QString, QString>>(&data);
}

void KeychainAccountManager::writeMap(const QString &accountName, const QMap<QString, QString> &map)
{
    const auto data = serialize<QMap<QString, QString>>(map);

    auto job = new QKeychain::WritePasswordJob(QStringLiteral("KIO GDrive"));
    job->setKey(accountName);
    job->setBinaryData(data);
    runKeychainJob(job);
}

void KeychainAccountManager::runKeychainJob(QKeychain::Job *job)
{
    QObject::connect(job, &QKeychain::Job::finished, [](QKeychain::Job *job) {
        switch (job->error()) {
        case QKeychain::NoError:
            return;
        case QKeychain::EntryNotFound:
            qCDebug(GDRIVE) << "Keychain job could not find key" << job->key();
            return;
        case QKeychain::CouldNotDeleteEntry:
            qCDebug(GDRIVE) << "Keychain job could not delete key" << job->key();
            return;
        case QKeychain::AccessDenied:
        case QKeychain::AccessDeniedByUser:
            qCDebug(GDRIVE) << "Keychain job could not access the system keychain";
            break;
        default:
            qCDebug(GDRIVE) << "Keychain job failed:" << job->error() << "-" << job->errorString();
            return;
        }
    });

    QEventLoop eventLoop;
    QObject::connect(job, &QKeychain::Job::finished, &eventLoop, &QEventLoop::quit);
    job->start();
    eventLoop.exec();
}

void KeychainAccountManager::removeAccount(const QString &accountName)
{
    auto job = new QKeychain::DeletePasswordJob(QStringLiteral("KIO GDrive"));
    job->setKey(accountName);
    runKeychainJob(job);
    removeAccountName(accountName);
}

template<typename T>
QByteArray KeychainAccountManager::serialize(const T &object)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_0);
    stream << object;

    return data;
}

template<typename T>
T KeychainAccountManager::deserialize(QByteArray *data)
{
    if (!data) {
        return {};
    }

    QDataStream stream(data, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_5_0);

    T object;
    stream >> object;

    return object;
}
