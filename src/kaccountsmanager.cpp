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

KGAPI2::AccountPtr KAccountsManager::account(const QString &accountName)
{
    if (!accountName.isEmpty() && m_accounts.contains(accountName)) {
        return m_accounts.value(accountName);
    }

    // TODO
    return {};
}

KGAPI2::AccountPtr KAccountsManager::refreshAccount(const KGAPI2::AccountPtr &account)
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
