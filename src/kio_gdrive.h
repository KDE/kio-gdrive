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
#include <KIO/WorkerBase>

#include <memory>

class AbstractAccountManager;

class QTemporaryFile;

namespace KGAPI2
{
class Job;
}

class KIOGDrive : public KIO::WorkerBase
{
public:
    enum Action {
        Success,
        Fail,
        Restart,
    };

    explicit KIOGDrive(const QByteArray &protocol, const QByteArray &pool_socket, const QByteArray &app_socket);
    ~KIOGDrive() override;

    virtual KIO::WorkerResult openConnection() Q_DECL_OVERRIDE;
    virtual KIO::WorkerResult listDir(const QUrl &url) Q_DECL_OVERRIDE;
    virtual KIO::WorkerResult mkdir(const QUrl &url, int permissions) Q_DECL_OVERRIDE;

    virtual KIO::WorkerResult stat(const QUrl &url) Q_DECL_OVERRIDE;
    virtual KIO::WorkerResult get(const QUrl &url) Q_DECL_OVERRIDE;
    virtual KIO::WorkerResult put(const QUrl &url, int permissions, KIO::JobFlags flags) Q_DECL_OVERRIDE;

    virtual KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) Q_DECL_OVERRIDE;
    virtual KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) Q_DECL_OVERRIDE;
    virtual KIO::WorkerResult del(const QUrl &url, bool isfile) Q_DECL_OVERRIDE;

    virtual KIO::WorkerResult mimetype(const QUrl &url) Q_DECL_OVERRIDE;
    KIO::WorkerResult fileSystemFreeSpace(const QUrl &url) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(KIOGDrive)

    enum PathFlags {
        None = 0,
        PathIsFolder = 1,
        PathIsFile = 2,
    };

    enum class FetchEntryFlags {
        None = 0,
        CurrentDir = 1,
    };

    static KIO::UDSEntry newAccountUDSEntry();
    static KIO::UDSEntry sharedWithMeUDSEntry();
    static KIO::UDSEntry accountToUDSEntry(const QString &accountName);
    static KIO::UDSEntry sharedDriveToUDSEntry(const KGAPI2::Drive::DrivesPtr &sharedDrive);

    [[nodiscard]] KIO::WorkerResult listAccounts();
    [[nodiscard]] KIO::WorkerResult createAccount();

    [[nodiscard]] KIO::WorkerResult listSharedDrivesRoot(const QUrl &url);
    [[nodiscard]] KIO::WorkerResult createSharedDrive(const QUrl &url);
    [[nodiscard]] KIO::WorkerResult deleteSharedDrive(const QUrl &url);
    [[nodiscard]] KIO::WorkerResult statSharedDrive(const QUrl &url);
    [[nodiscard]] KIO::UDSEntry fetchSharedDrivesRootEntry(const QString &accountId, FetchEntryFlags flags = FetchEntryFlags::None);

    [[nodiscard]] std::pair<KIO::WorkerResult, QString> resolveFileIdFromPath(const QString &path, PathFlags flags = None);
    QString resolveSharedDriveId(const QString &idOrName, const QString &accountId);

    struct Result {
        Action action;
        int error = KJob::NoError;
        QString errorString;

        static Result fail(int error, QString errorString)
        {
            return {Fail, error, errorString};
        }

        static Result success()
        {
            return {Success, KJob::NoError, QString()};
        }

        static Result restart()
        {
            return {Restart, KJob::NoError, QString()};
        }
    };

    [[nodiscard]] Result handleError(const KGAPI2::Job &job, const QUrl &url);
    KIO::UDSEntry fileToUDSEntry(const KGAPI2::Drive::FilePtr &file, const QString &path) const;
    QUrl fileToUrl(const KGAPI2::Drive::FilePtr &file, const QString &path) const;

    KGAPI2::AccountPtr getAccount(const QString &accountName);

    [[nodiscard]] std::pair<KIO::WorkerResult, QString> rootFolderId(const QString &accountId);

    [[nodiscard]] KIO::WorkerResult putUpdate(const QUrl &url);
    [[nodiscard]] KIO::WorkerResult putCreate(const QUrl &url);
    [[nodiscard]] KIO::WorkerResult readPutData(QTemporaryFile &tmpFile, KGAPI2::Drive::FilePtr &file);

    /**
     * Executes a KGAPI2::Job in an event loop, retrying the job until success or failure.
     * If the Job fails a WorkerResult with an appropriate error message will be returned.
     *
     * @return Whether @p job succeeded.
     */
    [[nodiscard]] KIO::WorkerResult runJob(KGAPI2::Job &job, const QUrl &url, const QString &accountId);

    std::unique_ptr<AbstractAccountManager> m_accountManager;
    PathCache m_cache;

    QMap<QString /* account */, QString /* rootId */> m_rootIds;
};

#endif // KIO_GDRIVE_H
