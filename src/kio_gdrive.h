/*
 * Copyright (C) 2013 - 2014 Daniel Vrátil <dvratil@redhat.com>
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

#include <KGAPI/Types>
#include <KGAPI/Account>

#include "accountmanager.h"
#include "pathcache.h"

Q_DECLARE_LOGGING_CATEGORY(LOG_KIO_GDRIVE)

class QTemporaryFile;
namespace KGAPI2
{
class Job;
}

class KIOGDrive : public KIO::SlaveBase
{
public:
    enum Action {
        Success,
        Fail,
        Restart
    };

    explicit KIOGDrive(const QByteArray &protocol,
                       const QByteArray &pool_socket,
                       const QByteArray &app_socket);
    virtual ~KIOGDrive();

    virtual void openConnection() Q_DECL_OVERRIDE;
    virtual void listDir(const QUrl &url) Q_DECL_OVERRIDE;
    virtual void mkdir(const QUrl &url, int permissions) Q_DECL_OVERRIDE;

    virtual void stat(const QUrl &url) Q_DECL_OVERRIDE;
    virtual void get(const QUrl &url) Q_DECL_OVERRIDE;
    virtual void put(const QUrl &url, int permissions, KIO::JobFlags flags) Q_DECL_OVERRIDE;

    virtual void copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) Q_DECL_OVERRIDE;
    virtual void rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) Q_DECL_OVERRIDE;
    virtual void del(const QUrl &url, bool isfile) Q_DECL_OVERRIDE;

    virtual void mimetype(const QUrl &url) Q_DECL_OVERRIDE;

private:
    enum PathFlags {
        None = 0,
        PathIsFolder = 1,
        PathIsFile = 2
    };

    void listAccounts();
    void createAccount();

    QString resolveFileIdFromPath(const QString &path, PathFlags flags = None);

    Action handleError(KGAPI2::Job *job, const QUrl &url);
    KIO::UDSEntry fileToUDSEntry(const KGAPI2::Drive::FilePtr &file, const QString &path) const;

    QStringList pathComponents(const QString &path) const;
    QStringList pathComponents(const QUrl &url) const {
        return pathComponents(url.adjusted(QUrl::StripTrailingSlash).path());
    }

    bool isRoot(const QUrl &url) const;
    bool isAccountRoot(const QUrl &url) const;
    QString accountFromPath(const QUrl &url) const;

    KGAPI2::AccountPtr getAccount(const QString &accountName) {
        return m_accountManager.account(accountName);
    }

    QString rootFolderId(const QString &accountId);

    bool putUpdate(const QUrl &url, const QString &accountId, const QStringList &pathComponents);
    bool putCreate(const QUrl &url, const QString &accountId, const QStringList &pathComponents);
    bool readPutData(QTemporaryFile &tmpFile);

    AccountManager m_accountManager;
    PathCache m_cache;

    QMap<QString /* account */, QString /* rootId */> m_rootIds;

};

#endif // GDRIVESLAVE_H
