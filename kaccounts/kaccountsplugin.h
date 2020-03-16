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

#include <KAccounts/KAccountsDPlugin>

class GoogleDrivePlugin : public KAccountsDPlugin
{
    Q_OBJECT
public:
    explicit GoogleDrivePlugin(QObject *parent, const QVariantList &args);

public slots:
    void onAccountCreated(const Accounts::AccountId accountId, const Accounts::ServiceList &serviceList) override;
    void onAccountRemoved(const Accounts::AccountId accountId) override;
    void onServiceEnabled(const Accounts::AccountId accountId, const Accounts::Service &service) override;
    void onServiceDisabled(const Accounts::AccountId accountId, const Accounts::Service &service) override;
};
