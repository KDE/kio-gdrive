/*
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "kaccountsmanager.h"
#include "gdrivedebug.h"
#include "gdrivehelper.h"

#include <Accounts/Manager>
#include <Accounts/Provider>
#include <KAccounts/Core>
#include <KAccounts/GetCredentialsJob>
#include <KGAPI/Account>

#include <QProcess>
#include <QStandardPaths>

using namespace KGAPI2;

KAccountsManager::KAccountsManager()
{
    loadAccounts();
}

KAccountsManager::~KAccountsManager()
{
}

AccountPtr KAccountsManager::account(const QString &accountName)
{
    const auto accounts = m_accounts.values();
    for (const auto &account : accounts) {
        if (account->accountName() == accountName) {
            return account;
        }
    }

    return AccountPtr(new Account());
}

AccountPtr KAccountsManager::createAccount()
{
    if (QStandardPaths::findExecutable(QStringLiteral("kcmshell6")).isEmpty()) {
        return AccountPtr(new Account());
    }

    const auto oldAccounts = accounts();

    QProcess process;
    process.start(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_kaccounts")});
    qCDebug(GDRIVE) << "Waiting for kcmshell process...";
    if (process.waitForFinished(-1)) {
        loadAccounts();
    }

    const auto newAccounts = accounts();
    for (const auto &accountName : newAccounts) {
        if (oldAccounts.contains(accountName)) {
            continue;
        }

        // The KCM allows to add more than one account, but we can return only one from here.
        // So we just return the first new account in the set.
        qCDebug(GDRIVE) << "New account successfully created:" << accountName;
        return account(accountName);
    }

    // No accounts at all or no new account(s).
    qCDebug(GDRIVE) << "No new account created.";
    return AccountPtr(new Account());
}

AccountPtr KAccountsManager::refreshAccount(const AccountPtr &account)
{
    const QString accountName = account->accountName();
    for (auto it = m_accounts.constBegin(); it != m_accounts.constEnd(); ++it) {
        if (it.value()->accountName() != accountName) {
            continue;
        }

        const auto id = it.key();
        qCDebug(GDRIVE) << "Refreshing" << accountName;
        auto gapiAccount = getAccountCredentials(id, accountName);
        m_accounts.insert(id, gapiAccount);
        return gapiAccount;
    }

    return {};
}

void KAccountsManager::removeAccount(const QString &accountName)
{
    if (!accounts().contains(accountName)) {
        return;
    }

    for (auto it = m_accounts.constBegin(); it != m_accounts.constEnd(); ++it) {
        if (it.value()->accountName() != accountName) {
            continue;
        }

        auto manager = KAccounts::accountsManager();
        auto account = Accounts::Account::fromId(manager, it.key());
        Q_ASSERT(account->displayName() == accountName);
        qCDebug(GDRIVE) << "Going to remove account:" << account->displayName();
        account->selectService(manager->service(QStringLiteral("google-drive")));
        account->setEnabled(false);
        account->sync();
        return;
    }
}

QSet<QString> KAccountsManager::accounts()
{
    auto accountNames = QSet<QString>();

    const auto accounts = m_accounts.values();
    for (const auto &account : accounts) {
        accountNames << account->accountName();
    }

    return accountNames;
}

void KAccountsManager::loadAccounts()
{
    m_accounts.clear();

    auto manager = KAccounts::accountsManager();
    const auto enabledIDs = manager->accountListEnabled();
    for (const auto id : enabledIDs) {
        auto account = manager->account(id);
        if (account->providerName() != QLatin1String("google")) {
            continue;
        }
        qCDebug(GDRIVE) << "Found google-provided account:" << account->displayName();
        const auto services = account->enabledServices();
        for (const auto &service : services) {
            if (service.name() != QLatin1String("google-drive")) {
                continue;
            }
            qCDebug(GDRIVE) << account->displayName() << "supports gdrive!";

            auto gapiAccount = getAccountCredentials(id, account->displayName());
            m_accounts.insert(id, gapiAccount);
        }
    }
}

AccountPtr KAccountsManager::getAccountCredentials(Accounts::AccountId id, const QString &displayName)
{
    auto job = new KAccounts::GetCredentialsJob(id, nullptr);
    job->exec();
    if (job->error()) {
        qCWarning(GDRIVE) << "GetCredentialsJob failed:" << job->errorString();
    }

    auto gapiAccount = AccountPtr(new Account(displayName,
                                              job->credentialsData().value(QStringLiteral("AccessToken")).toString(),
                                              job->credentialsData().value(QStringLiteral("RefreshToken")).toString()));

    const auto scopes = job->credentialsData().value(QStringLiteral("Scope")).toStringList();
    for (const auto &scope : scopes) {
        gapiAccount->addScope(QUrl::fromUserInput(scope));
    }

    qCDebug(GDRIVE) << "Got account credentials for:" << gapiAccount->accountName() << ", accessToken:" << GDriveHelper::elideToken(gapiAccount->accessToken())
                    << ", refreshToken:" << GDriveHelper::elideToken(gapiAccount->refreshToken());

    return gapiAccount;
}
