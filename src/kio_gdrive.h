/*
 * SPDX-FileCopyrightText: 2013-2014 Daniel Vrátil <dvratil@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef KIO_GDRIVE_H
#define KIO_GDRIVE_H

#include "pathcache.h"

#include <KGAPI/Account>
#include <KGAPI/Types>
#include <KIO/SlaveBase>

#include <memory>

class AbstractAccountManager;

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

protected:
    void virtual_hook(int id, void *data) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(KIOGDrive)

    enum PathFlags {
        None = 0,
        PathIsFolder = 1,
        PathIsFile = 2
    };

    enum class FetchEntryFlags {
        None = 0,
        CurrentDir = 1
    };

    static KIO::UDSEntry newAccountUDSEntry();
    static KIO::UDSEntry accountToUDSEntry(const QString &accountName);
    static KIO::UDSEntry sharedDriveToUDSEntry(const KGAPI2::Drive::DrivesPtr &sharedDrive);

    void listAccounts();
    void createAccount();

    void listSharedDrivesRoot(const QUrl &url);
    bool createSharedDrive(const QUrl &url);
    bool deleteSharedDrive(const QUrl &url);
    void statSharedDrive(const QUrl &url);
    KIO::UDSEntry fetchSharedDrivesRootEntry(const QString &accountId, FetchEntryFlags flags = FetchEntryFlags::None);

    QString resolveFileIdFromPath(const QString &path, PathFlags flags = None);
    QString resolveSharedDriveId(const QString &idOrName, const QString &accountId);

    Action handleError(const KGAPI2::Job &job, const QUrl &url);
    KIO::UDSEntry fileToUDSEntry(const KGAPI2::Drive::FilePtr &file, const QString &path) const;
    QUrl fileToUrl(const KGAPI2::Drive::FilePtr &file, const QString &path) const;

    void fileSystemFreeSpace(const QUrl &url);

    KGAPI2::AccountPtr getAccount(const QString &accountName);

    QString rootFolderId(const QString &accountId);

    bool putUpdate(const QUrl &url);
    bool putCreate(const QUrl &url);
    bool readPutData(QTemporaryFile &tmpFile, KGAPI2::Drive::FilePtr &file);

    /**
     * Executes a KGAPI2::Job in an event loop, retrying the job until success or failure.
     * If the Job fails, SlaveBase::error() with an appropriate error message will be called.
     *
     * @return Whether @p job succeeded.
     */
    bool runJob(KGAPI2::Job &job, const QUrl &url, const QString &accountId);

    std::unique_ptr<AbstractAccountManager> m_accountManager;
    PathCache m_cache;

    QMap<QString /* account */, QString /* rootId */> m_rootIds;

};

#endif // KIO_GDRIVE_H
