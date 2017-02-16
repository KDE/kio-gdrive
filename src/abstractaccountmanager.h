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

#pragma once

#include <QSet>

#include <KGAPI/Account>
#include <KIO/UDSEntry>

class AbstractAccountManager
{
public:
    virtual ~AbstractAccountManager();

    /**
     * @return Pointer to the account for @p accountName.
     * The pointer is valid only if @p accountName is in accounts().
     * @see accounts()
     */
    virtual KGAPI2::AccountPtr account(const QString &accountName) = 0;

    virtual KGAPI2::AccountPtr refreshAccount(const KGAPI2::AccountPtr &account) = 0;

    /**
     * Remove @p accountName from accounts().
     * @see accounts()
     */
    virtual void removeAccount(const QString &accountName) = 0;

    /**
     * @return The gdrive accounts managed by this object.
     */
    virtual QSet<QString> accounts() = 0;
};

