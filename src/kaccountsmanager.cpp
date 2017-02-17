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

using namespace KGAPI2;

KAccountsManager::KAccountsManager()
{
    loadAccounts();
}

KAccountsManager::~KAccountsManager()
{}

AccountPtr KAccountsManager::account(const QString &accountName)
{
    return m_accounts.value(accountName);
}

AccountPtr KAccountsManager::refreshAccount(const AccountPtr &account)
{
    // TODO
    return {};
}

void KAccountsManager::removeAccount(const QString &accountName)
{
    m_accounts.remove(accountName);
}

QSet<QString> KAccountsManager::accounts()
{
    auto accounts = QSet<QString>();

    const auto names = m_accounts.keys();
    for (const auto &name : names) {
        accounts << name;
    }

    return accounts;
}

void KAccountsManager::loadAccounts()
{
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

            m_accounts.insert(account->displayName(), gapiAccount);
        }
    }
}
