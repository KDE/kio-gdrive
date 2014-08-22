/*
 * Copyright (C) 2013 - 2014  Daniel Vrátil <dvratil@redhat.com>
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

#include "kio_gdrive.h"
#include "gdrivehelper.h"

#include <QtGui/QApplication>

#include <KDE/KComponentData>
#include <KDE/KDebug>
#include <KDE/KWallet/Wallet>
#include <KDE/KTemporaryFile>
#include <KIO/Job>
#include <KIO/AccessManager>

#include <LibKGAPI2/Account>
#include <LibKGAPI2/AuthJob>
#include <LibKGAPI2/Drive/ChildReference>
#include <LibKGAPI2/Drive/ChildReferenceFetchJob>
#include <LibKGAPI2/Drive/ChildReferenceCreateJob>
#include <LibKGAPI2/Drive/File>
#include <LibKGAPI2/Drive/FileCopyJob>
#include <LibKGAPI2/Drive/FileCreateJob>
#include <LibKGAPI2/Drive/FileModifyJob>
#include <LibKGAPI2/Drive/FileTrashJob>
#include <LibKGAPI2/Drive/FileFetchJob>
#include <LibKGAPI2/Drive/FileFetchContentJob>
#include <LibKGAPI2/Drive/FileSearchQuery>
#include <LibKGAPI2/Drive/ParentReference>
#include <LibKGAPI2/Drive/Permission>

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

using namespace KGAPI2;
using namespace Drive;


#define RUN_KGAPI_JOB(job) \
    RUN_KGAPI_JOB_PARAMS(job, url, accountId)
#define RUN_KGAPI_JOB_PARAMS(job, url, accountId) \
    RUN_KGAPI_JOB_IMPL(job, url, accountId, )
#define RUN_KGAPI_JOB_RETVAL(job, retval) \
    RUN_KGAPI_JOB_IMPL(job, url, accountId, retval)

#define RUN_KGAPI_JOB_IMPL(job, url, accountId, retval) \
{ \
    KIOGDrive::Action action = KIOGDrive::Fail; \
    Q_FOREVER { \
        kDebug() << "Running job with accessToken" << job.account()->accessToken(); \
        QEventLoop eventLoop; \
        QObject::connect(&job, SIGNAL(finished(KGAPI2::Job*)), \
                          &eventLoop, SLOT(quit())); \
        eventLoop.exec(); \
        action = handleError(&job, url); \
        if (action == KIOGDrive::Success) { \
            break; \
        } else if (action == KIOGDrive::Fail) { \
            return retval; \
        } \
        job.setAccount(getAccount(accountId)); \
        job.restart(); \
    }; \
}

extern "C"
{
    int KDE_EXPORT kdemain(int argc, char **argv)
    {
        QApplication app(argc, argv);
        KComponentData("kio_gdrive", "kdelibs4");

        if (argc != 4) {
             fprintf(stderr, "Usage: kio_gdrive protocol domain-socket1 domain-socket2\n");
             exit(-1);
        }

        KIOGDrive slave(argv[1], argv[2], argv[3]);
        slave.dispatchLoop();
        return 0;
    }
}

KIOGDrive::KIOGDrive(const QByteArray &protocol, const QByteArray &pool_socket,
                      const QByteArray &app_socket):
    SlaveBase("gdrive", pool_socket, app_socket)
{
    Q_UNUSED(protocol);

    kDebug() << "GDrive ready";
}

KIOGDrive::~KIOGDrive()
{
    closeConnection();
}

KIOGDrive::Action KIOGDrive::handleError(KGAPI2::Job *job, const KUrl &url)
{
    kDebug() << job->error() << job->errorString();

    switch (job->error()) {
        case KGAPI2::OK:
        case KGAPI2::NoError:
            return Success;
        case KGAPI2::AuthCancelled:
        case KGAPI2::AuthError:
            error(KIO::ERR_COULD_NOT_LOGIN, url.prettyUrl());
            return Fail;
        case KGAPI2::Unauthorized: {
            const AccountPtr oldAccount = job->account();
            const AccountPtr account = m_accountManager.refreshAccount(oldAccount);
            if (!account) {
                error(KIO::ERR_COULD_NOT_LOGIN, url.prettyUrl());
                return Fail;
            }
            return Restart;
        }
        case KGAPI2::Forbidden:
            error(KIO::ERR_ACCESS_DENIED, url.prettyUrl());
            return Fail;
        case KGAPI2::NotFound:
            error(KIO::ERR_DOES_NOT_EXIST, url.prettyUrl());
            return Fail;
        case KGAPI2::NoContent:
            error(KIO::ERR_NO_CONTENT, url.prettyUrl());
            return Fail;
        case KGAPI2::QuotaExceeded:
            error(KIO::ERR_DISK_FULL, url.prettyUrl());
            return Fail;
        default:
            error(KIO::ERR_SLAVE_DEFINED, job->errorString());
            return Fail;
    }

    return Fail;
}

KIO::UDSEntry KIOGDrive::fileToUDSEntry(const FilePtr &origFile) const
{
    KIO::UDSEntry entry;
    bool isFolder = false;

    FilePtr file = origFile;
    if (GDriveHelper::isGDocsDocument(file)) {
        GDriveHelper::convertFromGDocs(file);
    }

    entry.insert(KIO::UDSEntry::UDS_NAME, file->title());
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, file->title());

    if (file->isFolder()) {
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
        isFolder = true;
    } else {
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
        entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, file->mimeType());
        entry.insert(KIO::UDSEntry::UDS_SIZE, file->fileSize());
    }

    entry.insert(KIO::UDSEntry::UDS_CREATION_TIME, file->createdDate().toTime_t());
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, file->modifiedDate().toTime_t());
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, file->lastViewedByMeDate().toTime_t());
    if (!file->ownerNames().isEmpty()) {
        entry.insert(KIO::UDSEntry::UDS_USER, file->ownerNames().first());
    }
    entry.insert(KIO::UDSEntry::UDS_HIDDEN, file->labels()->hidden());

    if (!isFolder) {
        if (file->editable()) {
            entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        } else {
            entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH);
        }
    } else {
        entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }

    return entry;
}

void KIOGDrive::openConnection()
{
    kDebug() << "Ready to talk to GDrive";
}




QStringList KIOGDrive::pathComponents(const QString &path) const
{
    return path.split(QLatin1Char('/'), QString::SkipEmptyParts);
}

QString KIOGDrive::accountFromPath(const KUrl& url) const
{
    const QStringList components = pathComponents(url);
    if (components.isEmpty()) {
        return QString();
    }
    return components[0];
}

bool KIOGDrive::isRoot(const KUrl& url) const
{
    return pathComponents(url).length() == 0;
}


bool KIOGDrive::isAccountRoot(const KUrl& url) const
{
    return pathComponents(url).length() == 1;
}


void KIOGDrive::createAccount()
{
    const KGAPI2::AccountPtr account = m_accountManager.account(QString());
    redirection(KUrl(QString::fromLatin1("gdrive:/%1").arg(account->accountName())));
    finished();
}

void KIOGDrive::listAccounts()
{
    const QString accounts = m_accountManager.accounts();
    if (accounts.isEmpty()) {
        createAccount();
        return;
    }

    for (const QString &account : accounts) {
        const KIO::UDSEntry entry = AccountManager::accountToUDSEntry(account);
        listEntry(entry, false);
    }
    KIO::UDSEntry newAccountEntry;
    newAccountEntry.insert(KIO::UDSEntry::UDS_NAME, QLatin1String("new-account"));
    newAccountEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("New account"));
    newAccountEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    newAccountEntry.insert(KIO::UDSEntry::UDS_ICON_NAME, QLatin1String("list-add-user"));
    listEntry(newAccountEntry, false);
    listEntry(KIO::UDSEntry(), true);
    finished();
    return;
}

QString KIOGDrive::resolveFileIdFromPath(const QString &path, bool isFolder)
{
    QString fileId = m_cache.idForPath(path);
    if (!fileId.isEmpty()) {
        return fileId;
    }

    const int index = path.lastIndexOf(QLatin1Char('/'));
    if (index < 1) {
        return QLatin1String("root");
    }
    const QString fileName = path.midRef(index + 1);
    const QString parentPath = m_cache.idForPath(path.midRef(0, index - 1));

    // Try to recursively resolve ID of parent path - either from cache, or by
    // querying Google
    const QString parentId = resolveFileIdFromPath(parentPath, true);

    FileSearchQuery query;
    query.addQuery(FileSearchQuery::MimeType,
                   isFolder ? FileSearchQuery::Equals : FileSearchQuery::NotEquals,
                   GDriveHelper::folderMimeType());
    query.addQuery(FileSearchQuery::Title, FileSearchQuery::Equals, x);
    query.addQuery(FileSearchQuery::Parents, FileSearchQuery::In, parentId);

    const QString accountId = accountFromPath(path);
    FileFetchJob fetchJob(query, getAccount(accountId));
    fetchJob.setFields(FileFetchJob::Id | FileFetchJob::Title);
    RUN_KGAPI_JOB_RETVAL(fetchJob, QString());

    const ObjectsList objects = fetchJob.items();
    if (objects.count() != 1) {
        return QString();
    }

    const FilePtr file = objects[0].dynamicCast<File>();
    m_cache.insertPath(path, file->id());

    return file->id();
}



void KIOGDrive::listDir(const KUrl &url)
{
    kDebug() << url;

    if (isRoot(url)) {
        listAccounts();
        return;
    }

    const QString accountId = accountFromPath(url);
    if (accountId == QLatin1String("new-account")) {
        createAccount();
        return;
    }

    const QString folderName = url.fileName();
    const bool listingTrash = (folderName == QLatin1String("trash"));

    QString folderId;
    if (isAccountRoot(url)) {
        folderId = QLatin1String("root");
    } else {
        folderId = m_cache.idForPath(url.path());
        if (folderId.isEmpty()) {
            folderId = resolveFileIdFromPath(url.path(KUrl::RemoveTrailingSlash), true);
        }
        if (folderId.isEmpty()) {
            error(KIO::ERR_DOES_NOT_EXIST, url.path());
            return;
        }
    }

    FileSearchQuery query;
    if (listingTrash) {
        query.addQuery(FileSearchQuery::Trashed, FileSearchQuery::Equals, true);
    } else {
        query.addQuery(FileSearchQuery::Parents, FileSearchQuery::In, folderId);
        query.addQuery(FileSearchQuery::Trashed, FileSearchQuery::Equals, false);
    }
    FileFetchJob fileFetchJob(query, getAccount(accountId));
    fileFetchJob.setFields((FileFetchJob::BasicFields & ~FileFetchJob::Permissions)
                            | FileFetchJob::Labels
                            | FileFetchJob::ExportLinks
                            | FileFetchJob::LastViewedByMeDate);
    RUN_KGAPI_JOB(fileFetchJob)

    ObjectsList objects = fileFetchJob.items();
    Q_FOREACH (const ObjectPtr &object, objects) {
        const FilePtr file = object.dynamicCast<File>();

        const KIO::UDSEntry entry = fileToUDSEntry(file);
        listEntry(entry, false);

        m_cache.insertPath(url.path(KUrl::AddTrailingSlash) + file->title(), file->id());
    }

    if (isAccountRoot(url)) {
        KIO::UDSEntry trashEntry;
        trashEntry.insert(KIO::UDSEntry::UDS_NAME, QLatin1String("trash"));
        trashEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("Trash"));
        trashEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        trashEntry.insert(KIO::UDSEntry::UDS_ICON_NAME, QLatin1String("user-trash"));
        listEntry(trashEntry, false);
    }

    listEntry(KIO::UDSEntry(), true);
    finished();
}


void KIOGDrive::mkdir(const KUrl &url, int permissions)
{
    // NOTE: We deliberately ignore the permissions field here, because GDrive
    // does not recognize any privileges that could be mapped to standard UNIX
    // file permissions.
    Q_UNUSED(permissions);

    kDebug() << url << permissions;

    const QString folderName = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    FilePtr file(new File());
    file->setTitle(folderName);
    file->setMimeType(File::folderMimeType());

    const KUrl parentUrl = url.upUrl();
    QString parentId;
    if (!parentUrl.path(KUrl::RemoveTrailingSlash).isEmpty()) {
        parentId = lastPathComponent(parentUrl);
    } else {
        parentId = QLatin1String("root");
    }

    ParentReferencePtr parent(new ParentReference(parentId));
    file->setParents(ParentReferencesList() << parent);

    FileCreateJob createJob(file, getAccount(accountId));
    RUN_KGAPI_JOB(createJob)

    finished();
}



void KIOGDrive::stat(const KUrl &url)
{
    kDebug() << url;

    const QString fileId = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    // If this is an account, don't query Google!
    if (!accountId.isEmpty() && fileId == QLatin1String("root")) {
        const KIO::UDSEntry entry = AccountManager::accountToUDSEntry(accountId);
        statEntry(entry);
        finished();
        return;
    }

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    RUN_KGAPI_JOB(fileFetchJob)

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.fileName());
        return;
    }

    const FilePtr file = objects.first().dynamicCast<File>();
    const KIO::UDSEntry entry = fileToUDSEntry(file);

    statEntry(entry);
    finished();
}

void KIOGDrive::get(const KUrl &url)
{
    kDebug() << url;

    const QString fileId = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    fileFetchJob.setFields(FileFetchJob::Id
                            | FileFetchJob::MimeType
                            | FileFetchJob::ExportLinks
                            | FileFetchJob::DownloadUrl);
    RUN_KGAPI_JOB(fileFetchJob)

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.fileName());
        return;
    }

    FilePtr file = objects.first().dynamicCast<File>();
    QUrl downloadUrl;
    if (GDriveHelper::isGDocsDocument(file)) {
        downloadUrl = GDriveHelper::convertFromGDocs(file);
    } else {
        downloadUrl = file->downloadUrl();
    }

    mimeType(file->mimeType());

    FileFetchContentJob contentJob(downloadUrl, getAccount(accountId));
    RUN_KGAPI_JOB(contentJob)

    data(contentJob.data());
    finished();
}

void KIOGDrive::put(const KUrl &url, int permissions, KIO::JobFlags flags)
{
    kDebug() << url << permissions << flags;

    const QString fileName = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    ParentReferencesList parentReferences;
    const QStringList paths = url.path(KUrl::RemoveTrailingSlash).split(QLatin1Char('/'), QString::SkipEmptyParts);
    kDebug() << paths;
    if (paths.length() < 2) {
        error(KIO::ERR_ACCESS_DENIED, url.fileName());
        return;
    } else if (paths.length() == 2) {
        // Creating in root directory
    } else {
        const QString parentId = paths[paths.length() - 2];
        parentReferences << ParentReferencePtr(new ParentReference(parentId));
    }

    FilePtr file(new File);
    file->setTitle(fileName);
    file->setParents(parentReferences);
    /*
    if (hasMetaData(QLatin1String("modified"))) {
        const QString modified = metaData(QLatin1String("modified"));
        kDebug() << modified;
        file->setModifiedDate(KDateTime::fromString(modified, KDateTime::ISODate));
    }
    */

    // TODO: Instead of using a temp file, upload directly the raw data (requires
    // support in LibKGAPI)

    // TODO: For large files, switch to resumable upload and upload the file in
    // reasonably large chunks (requires support in LibKGAPI)

    // TODO: Support resumable upload (requires support in LibKGAPI)

    KTemporaryFile tempFile;
    tempFile.setPrefix(QLatin1String("gdrive"));
    if (!tempFile.open()) {
        error(KIO::ERR_COULD_NOT_WRITE, tempFile.fileName());
        return;
    }

    int result;
    do {
        QByteArray buffer;
        dataReq();
        result = readData(buffer);
        qint64 size = tempFile.write(buffer);
        if (size != buffer.size()) {
            error(KIO::ERR_COULD_NOT_WRITE, tempFile.fileName());
            return;
        }
    } while (result > 0);

    if (result == -1) {
        error(KIO::ERR_COULD_NOT_READ, url.fileName());
        return;
    }

    FileCreateJob createJob(tempFile.fileName(), file, getAccount(accountId));
    RUN_KGAPI_JOB(createJob)

    finished();
}



void KIOGDrive::copy(const KUrl &src, const KUrl &dest, int permissions, KIO::JobFlags flags)
{
    // NOTE: We deliberately ignore the permissions field here, because GDrive
    // does not recognize any privileges that could be mapped to standard UNIX
    // file permissions.
    Q_UNUSED(permissions);

    // NOTE: We deliberately ignore the flags field here, because the "overwrite"
    // flag would have no effect on GDrive, since file name don't have to be
    // unique. IOW if there is a file "foo.bar" and user copy-pastes into the
    // same directory, the FileCopyJob will succeed and a new file with the same
    // name will be created.
    Q_UNUSED(flags);

    const QString sourceFileId = lastPathComponent(src);
    const QString destFileName = lastPathComponent(dest);
    const QString sourceAccountId = accountFromPath(src);
    const QString destAccountId = accountFromPath(dest);

    // TODO: Does this actually happen, or does KIO treat our account name as host?
    if (sourceAccountId != destAccountId) {
        // KIO will fallback to get+post
        error(KIO::ERR_UNSUPPORTED_ACTION, src.fileName());
        return;
    }

    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    sourceFileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::ModifiedDate |
                                 FileFetchJob::LastViewedByMeDate | FileFetchJob::Description);
    RUN_KGAPI_JOB_PARAMS(sourceFileFetchJob, src, sourceAccountId)

    const ObjectsList objects = sourceFileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, src.fileName());
        return;
    }

    const FilePtr sourceFile = objects[0].dynamicCast<File>();

    ParentReferencesList destParentReferences;
    const QStringList paths = dest.path(KUrl::RemoveTrailingSlash).split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (paths.size() == 0) {
        // paths.size == 0 -> error, user is trying to copy to top-level gdrive:///
        error(KIO::ERR_ACCESS_DENIED, dest.fileName());
        return;
    } else if (paths.size() == 1) {
        // user is trying to copy to root -> keep destParentReferences empty
    } else if (paths.size() > 2) {
        // skip filename and extract the second-to-last component
        const QString destDirId = paths[paths.count() - 2];
        destParentReferences << ParentReferencePtr(new ParentReference(destDirId));
    }

    FilePtr destFile(new File);
    destFile->setTitle(destFileName);
    destFile->setModifiedDate(sourceFile->modifiedDate());
    destFile->setLastViewedByMeDate(sourceFile->lastViewedByMeDate());
    destFile->setDescription(sourceFile->description());
    destFile->setParents(destParentReferences);

    FileCopyJob copyJob(sourceFile, destFile, getAccount(sourceAccountId));
    RUN_KGAPI_JOB_PARAMS(copyJob, dest, sourceAccountId)

    finished();
}

void KIOGDrive::del(const KUrl &url, bool isfile)
{
    // FIXME: Verify that a single file cannot actually have multiple parent
    // references. If it can, then we need to be more careful: currently this
    // implementation will simply remove the file from all it's parents but
    // it actually should just remove the current parent reference

    // FIXME: Because of the above, we are not really deleting the file, but only
    // moving it to trash - so if users really really really wants to delete the
    // file, they have to go to GDrive web interface and delete it there. I think
    // that we should do the DELETE operation here, because for trash people have
    // their local trashes. This however requires fixing the first FIXME first,
    // otherwise we are risking severe data loss.

    kDebug() << url << isfile;

    const QString fileId = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    // If user tries to delete the account folder, remove the account from KWallet
    if (!isfile && fileId == QLatin1String("root") && !accountId.isEmpty()) {
        const KGAPI2::AccountPtr account = m_accountManager.account(accountId);
        if (!account) {
            error(KIO::ERR_DOES_NOT_EXIST, accountId);
            return;
        }
        m_accountManager.removeAccount(accountId);
        finished();
        return;
    }

    // GDrive allows us to delete entire directory even when it's not empty,
    // so we need to emulate the normal behavior ourselves by checking number of
    // child references
    if (!isfile) {
        ChildReferenceFetchJob referencesFetch(fileId, getAccount(accountId));
        RUN_KGAPI_JOB(referencesFetch);
        const bool isEmpty = !referencesFetch.items().count();

        if (!isEmpty && metaData("recurse") != QLatin1String("true")) {
            error(KIO::ERR_COULD_NOT_RMDIR, url.fileName());
            return;
        }
    }

    FileTrashJob trashJob(fileId, getAccount(accountId));
    RUN_KGAPI_JOB(trashJob)

    finished();

}

void KIOGDrive::rename(const KUrl &src, const KUrl &dest, KIO::JobFlags flags)
{
    kDebug() << src << dest << flags;

    const QString sourceFileId = lastPathComponent(src);
    const QString destFileName = lastPathComponent(dest);
    const QString sourceAccountId = accountFromPath(src);
    const QString destAccountId = accountFromPath(dest);

    // TODO: Does this actually happen, or does KIO treat our account name as host?
    if (sourceAccountId != destAccountId) {
        error(KIO::ERR_UNSUPPORTED_ACTION, src.fileName());
        return;
    }

    // We need to fetch ALL, so that we can do update later
    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    RUN_KGAPI_JOB_PARAMS(sourceFileFetchJob, src, sourceAccountId)

    const ObjectsList objects = sourceFileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, src.fileName());
        return;
    }

    const FilePtr sourceFile = objects[0].dynamicCast<File>();

    ParentReferencesList parentReferences = sourceFile->parents();
    const QStringList destPaths = dest.path(KUrl::RemoveTrailingSlash).split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (destPaths.size() == 0) {
        // paths.size == 0 -> error, user is trying to move to top-level gdrive:///
        error(KIO::ERR_ACCESS_DENIED, dest.fileName());
        return;
    } else if (destPaths.size() == 1) {
        // user is trying to move to root -> we are only renaming
    } else if (destPaths.size() > 2) {
        const QStringList srcPaths = src.path(KUrl::RemoveTrailingSlash).split(QLatin1Char('/'), QString::SkipEmptyParts);
        if (srcPaths.size() < 3) {
            // WTF?
            error(KIO::ERR_DOES_NOT_EXIST, src.fileName());
            return;
        }

        // skip filename and extract the second-to-last component
        const QString destDirId = destPaths[destPaths.count() - 2];
        const QString srcDirId = srcPaths[srcPaths.count() - 2];

        // Remove source from parent references
        auto iter = parentReferences.begin();
        bool removed = false;
        while (iter != parentReferences.end()) {
            const ParentReferencePtr ref = *iter;
            if (ref->id() == srcDirId) {
                parentReferences.erase(iter);
                removed = true;
                break;
            }
            ++iter;
        }
        if (!removed) {
            error(KIO::ERR_DOES_NOT_EXIST, src.fileName());
            return;
        }

        // Add destination to parent references
        parentReferences << ParentReferencePtr(new ParentReference(destDirId));
    }

    FilePtr destFile(sourceFile);
    destFile->setTitle(destFileName);
    destFile->setParents(parentReferences);

    FileModifyJob modifyJob(destFile, getAccount(sourceAccountId));
    modifyJob.setUpdateModifiedDate(true);
    RUN_KGAPI_JOB_PARAMS(modifyJob, dest, sourceAccountId)

    finished();
}

void KIOGDrive::mimetype(const KUrl &url)
{
    kDebug() << url;

    const QString fileId = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    fileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::MimeType);
    RUN_KGAPI_JOB(fileFetchJob)

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.fileName());
        return;
    }

    const FilePtr file = objects.first().dynamicCast<File>();
    mimeType(file->mimeType());
    finished();
}
