/*
 * Copyright (c) 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
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

#include "kaccountsmanager.h"
#include "gdrivedebug.h"

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
{}

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
    if (QStandardPaths::findExecutable(QStringLiteral("kcmshell5")).isEmpty()) {
        return AccountPtr(new Account());
    }

    const auto oldAccounts = accounts();

    QProcess process;
    process.start(QStringLiteral("kcmshell5"), {QStringLiteral("kcm_kaccounts")});
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
    Q_UNUSED(account)
    qCWarning(GDRIVE) << Q_FUNC_INFO << "not implemented.";
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

            auto job = new GetCredentialsJob(id, nullptr);
            job->exec();

            auto gapiAccount = AccountPtr(new Account(account->displayName(),
                                                      job->credentialsData().value(QStringLiteral("AccessToken")).toString(),
                                                      job->credentialsData().value(QStringLiteral("RefreshToken")).toString()));

            const auto scopes = job->credentialsData().value(QStringLiteral("Scope")).toStringList();
            for (const auto &scope : scopes) {
                gapiAccount->addScope(QUrl::fromUserInput(scope));
            }

            m_accounts.insert(id, gapiAccount);
        }
    }
}
