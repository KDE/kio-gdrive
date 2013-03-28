/*
 * Copyright (C) 2013  Daniel Vrátil <dvratil@redhat.com>
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

#ifndef GDRIVESLAVE_H
#define GDRIVESLAVE_H

#include <KIO/SlaveBase>

#include <LibKGAPI2/Account>

namespace KWallet
{
class Wallet;
}

class KIOGDrive : public KIO::SlaveBase
{
  public:
    explicit KIOGDrive( const QByteArray &protocol,
                        const QByteArray &pool_socket,
                        const QByteArray &app_socket );
    virtual ~KIOGDrive();

    virtual void openConnection();
    virtual void listDir(const KUrl &url);
    virtual void stat(const KUrl &url);

  private:
    KGAPI2::AccountPtr m_account;
    KWallet::Wallet *m_wallet;
};

#endif // GDRIVESLAVE_H
