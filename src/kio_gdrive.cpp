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
#include <LibKGAPI2/Drive/About>
#include <LibKGAPI2/Drive/AboutFetchJob>
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
        kDebug() << "Running job" << (&job) << "with accessToken" << job.account()->accessToken(); \
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

static QString joinSublist(const QStringList &strv, int start, int end, const QChar &joinChar)
{
    QString res;
    for (int i = start; i <= end; ++i) {
        res += strv[i];
        if (i < end) {
            res += joinChar;
        }
    }
    return res;
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

KIO::UDSEntry KIOGDrive::fileToUDSEntry(const FilePtr &origFile, const QString &path) const
{
    KIO::UDSEntry entry;
    bool isFolder = false;

    FilePtr file = origFile;
    if (GDriveHelper::isGDocsDocument(file)) {
        GDriveHelper::convertFromGDocs(file);
    }

    entry.insert(KIO::UDSEntry::UDS_NAME, file->title());
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, file->title());
    entry.insert(KIO::UDSEntry::UDS_COMMENT, file->description());

    if (file->isFolder()) {
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
        isFolder = true;
    } else {
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
        entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, file->mimeType());
        entry.insert(KIO::UDSEntry::UDS_SIZE, file->fileSize());
        entry.insert(KIO::UDSEntry::UDS_URL, QString::fromLatin1("gdrive://%1/%2?id=%3").arg(path, origFile->title(), origFile->id()));
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
    const QStringList accounts = m_accountManager.accounts();
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

class RecursionDepthCounter
{
public:
    RecursionDepthCounter()
    {
        ++sDepth;
    }
    ~RecursionDepthCounter()
    {
        --sDepth;
    }

    int depth() const
    {
        return sDepth;
    }

private:
    static int sDepth;
};

int RecursionDepthCounter::sDepth = 0;


QString KIOGDrive::resolveFileIdFromPath(const QString &path, PathFlags flags)
{
    kDebug() << path;
    RecursionDepthCounter recursion;
    if (recursion.depth() == 1) {
        m_cache.dump();
    }

    if (path.isEmpty()) {
        return QString();
    }

    QString fileId = m_cache.idForPath(path);
    if (!fileId.isEmpty()) {
        kDebug() << "Resolved" << path << "to" << fileId << "(from cache)";
        return fileId;
    }

    const QStringList components = pathComponents(path);
    Q_ASSERT(!components.isEmpty());
    if (components.size() == 1 || (components.size() == 2 && components[1] == QLatin1String("trash"))) {
        kDebug() << "Resolved" << path << "to \"root\"";
        return rootFolderId(components[0]);
    }

    QString parentName;
    Q_ASSERT(components.size() >= 2);
    const QString parentPath = joinSublist(components, 0, components.size() - 2, QLatin1Char('/'));
    // Try to recursively resolve ID of parent path - either from cache, or by
    // querying Google
    const QString parentId = resolveFileIdFromPath(parentPath, KIOGDrive::PathIsFolder);
    if (parentId.isEmpty()) {
        // We failed to resolve parent -> error
        return QString();
    }

    FileSearchQuery query;
    if (flags != KIOGDrive::None) {
        query.addQuery(FileSearchQuery::MimeType,
                       (flags & KIOGDrive::PathIsFolder ? FileSearchQuery::Equals : FileSearchQuery::NotEquals),
                       GDriveHelper::folderMimeType());
    }
    query.addQuery(FileSearchQuery::Title, FileSearchQuery::Equals, components.last());
    query.addQuery(FileSearchQuery::Parents, FileSearchQuery::In, parentId);
    query.addQuery(FileSearchQuery::Trashed, FileSearchQuery::Equals, components[1] == QLatin1String("trash"));

    const QString accountId = accountFromPath(path);
    FileFetchJob fetchJob(query, getAccount(accountId));
    fetchJob.setFields(FileFetchJob::Id | FileFetchJob::Title | FileFetchJob::Labels);
    QUrl url(path);
    RUN_KGAPI_JOB_RETVAL(fetchJob, QString());

    const ObjectsList objects = fetchJob.items();
    kDebug() << objects;
    if (objects.count() == 0) {
        kWarning() << "Failed to resolve" << path;
        return QString();
    }

    const FilePtr file = objects[0].dynamicCast<File>();

    m_cache.insertPath(path, file->id());

    kDebug() << "Resolved" << path << "to" << file->id() << "(from network)";
    return file->id();
}

QString KIOGDrive::rootFolderId(const QString &accountId)
{
    if (!m_rootIds.contains(accountId)) {
        AboutFetchJob aboutFetch(getAccount(accountId));
        QUrl url;
        RUN_KGAPI_JOB_RETVAL(aboutFetch, QString())

        const AboutPtr about = aboutFetch.aboutData();
        if (!about || about->rootFolderId().isEmpty()) {
            kWarning() << "Failed to obtain root ID";
            return QString();
        }

        m_rootIds.insert(accountId, about->rootFolderId());
        return about->rootFolderId();
    }

    return m_rootIds[accountId];
}

void KIOGDrive::listDir(const KUrl &url)
{
    kDebug() << url;

    const QString accountId = accountFromPath(url);
    if (accountId == QLatin1String("new-account")) {
        createAccount();
        return;
    }

    QString folderId;
    const QStringList components = pathComponents(url);
    if (components.isEmpty())  {
        listAccounts();
        return;
    } else if (components.size() == 1) {
        folderId = rootFolderId(accountId);
    } else {
        folderId = m_cache.idForPath(url.path());
        if (folderId.isEmpty()) {
            folderId = resolveFileIdFromPath(url.path(KUrl::RemoveTrailingSlash),
                                             KIOGDrive::PathIsFolder);
        }
        if (folderId.isEmpty()) {
            error(KIO::ERR_DOES_NOT_EXIST, url.path());
            return;
        }
    }

    FileSearchQuery query;
    query.addQuery(FileSearchQuery::Trashed, FileSearchQuery::Equals, false);
    query.addQuery(FileSearchQuery::Parents, FileSearchQuery::In, folderId);
    FileFetchJob fileFetchJob(query, getAccount(accountId));
    fileFetchJob.setFields((FileFetchJob::BasicFields & ~FileFetchJob::Permissions)
                            | FileFetchJob::Labels
                            | FileFetchJob::ExportLinks
                            | FileFetchJob::LastViewedByMeDate);
    RUN_KGAPI_JOB(fileFetchJob)

    ObjectsList objects = fileFetchJob.items();
    Q_FOREACH (const ObjectPtr &object, objects) {
        const FilePtr file = object.dynamicCast<File>();

        const KIO::UDSEntry entry = fileToUDSEntry(file, url.path(KUrl::RemoveTrailingSlash));
        listEntry(entry, false);

        m_cache.insertPath(url.path(KUrl::AddTrailingSlash) + file->title(), file->id());
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

    const QString accountId = accountFromPath(url);
    const QStringList components = pathComponents(url);
    QString parentId;
    // At least account and new folder name
    if (components.size() < 2) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    } else if (components.size() == 2) {
        parentId = rootFolderId(accountId);
    } else {
        const QString subpath = joinSublist(components, 0, components.size() - 2, QLatin1Char('/'));
        parentId = resolveFileIdFromPath(subpath, KIOGDrive::PathIsFolder);
    }

    if (parentId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    const QString folderName = components.last();

    FilePtr file(new File());
    file->setTitle(folderName);
    file->setMimeType(File::folderMimeType());

    ParentReferencePtr parent(new ParentReference(parentId));
    file->setParents(ParentReferencesList() << parent);

    FileCreateJob createJob(file, getAccount(accountId));
    RUN_KGAPI_JOB(createJob)

    finished();
}

void KIOGDrive::stat(const KUrl &url)
{
    kDebug() << url;

    const QString accountId = accountFromPath(url);
    const QStringList components = pathComponents(url);
    if (components.isEmpty()) {
        // TODO Can we stat() root?
        finished();
        return;
    } else if (components.size() == 1) {
        const KIO::UDSEntry entry = AccountManager::accountToUDSEntry(accountId);
        statEntry(entry);
        finished();
        return;
    }

    const QString fileId
        = url.hasQueryItem(QLatin1String("id"))
            ? url.queryItem(QLatin1String("id"))
            : resolveFileIdFromPath(url.path(KUrl::RemoveTrailingSlash),
                                    KIOGDrive::None);
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    RUN_KGAPI_JOB(fileFetchJob)

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    const FilePtr file = objects.first().dynamicCast<File>();
    if (file->labels()->trashed()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    const KIO::UDSEntry entry = fileToUDSEntry(file, joinSublist(components, 0, components.size() - 2, QLatin1Char('/')));

    statEntry(entry);
    finished();
}

void KIOGDrive::get(const KUrl &url)
{
    kDebug() << url;

    const QString accountId = accountFromPath(url);
    const QStringList components = pathComponents(url);

    if (components.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    } else if (components.size() == 1) {
        // You cannot GET an account folder!
        error(KIO::ERR_ACCESS_DENIED, url.path());
        return;
    }

    const QString fileId =
        url.hasQueryItem(QLatin1String("id"))
            ? url.queryItem(QLatin1String("id"))
            : resolveFileIdFromPath(url.path(KUrl::RemoveTrailingSlash),
                                    KIOGDrive::PathIsFile);
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

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

bool KIOGDrive::readPutData(KTemporaryFile &tempFile)
{
    // TODO: Instead of using a temp file, upload directly the raw data (requires
    // support in LibKGAPI)

    // TODO: For large files, switch to resumable upload and upload the file in
    // reasonably large chunks (requires support in LibKGAPI)

    // TODO: Support resumable upload (requires support in LibKGAPI)

    tempFile.setPrefix(QLatin1String("gdrive"));
    if (!tempFile.open()) {
        error(KIO::ERR_COULD_NOT_WRITE, tempFile.fileName());
        return false;
    }

    int result;
    do {
        QByteArray buffer;
        dataReq();
        result = readData(buffer);
        if (!buffer.isEmpty()) {
            qint64 size = tempFile.write(buffer);
            if (size != buffer.size()) {
                error(KIO::ERR_COULD_NOT_WRITE, tempFile.fileName());
                return false;
            }
        }
    } while (result > 0);
    tempFile.close();

    if (result == -1) {
        kWarning() << "Could not read source file" << tempFile.fileName();
        error(KIO::ERR_COULD_NOT_READ, QString());
        return false;
    }

    return true;
}

bool KIOGDrive::putUpdate(const KUrl &url, const QString &accountId, const QStringList &pathComponents)
{
    const QString fileId = url.queryItem(QLatin1String("id"));
    kDebug() << url << fileId;

    FileFetchJob fetchJob(fileId, getAccount(accountId));
    RUN_KGAPI_JOB_RETVAL(fetchJob, false)

    const ObjectsList objects = fetchJob.items();
    if (objects.size() != 1) {
        putCreate(url, accountId, pathComponents);
        return false;
    }

    const FilePtr file = objects[0].dynamicCast<File>();
    KTemporaryFile tmpFile;
    if (!readPutData(tmpFile)) {
        error(KIO::ERR_COULD_NOT_READ, url.path());
        return false;
    }

    FileModifyJob modifyJob(tmpFile.fileName(), file, getAccount(accountId));
    modifyJob.setUpdateModifiedDate(true);
    RUN_KGAPI_JOB_RETVAL(modifyJob, false)

    return true;
}

bool KIOGDrive::putCreate(const KUrl &url, const QString &accountId, const QStringList &components)
{
    kDebug() << url;
    ParentReferencesList parentReferences;
    if (components.size() < 2) {
        error(KIO::ERR_ACCESS_DENIED, url.path());
        return false;
    } else if (components.length() == 2) {
        // Creating in root directory
    } else {
        const QString parentId = resolveFileIdFromPath(joinSublist(components, 0, components.size() - 2, QLatin1Char('/')));
        if (parentId.isEmpty()) {
            error(KIO::ERR_DOES_NOT_EXIST, url.directory());
            return false;
        }
        parentReferences << ParentReferencePtr(new ParentReference(parentId));
    }

    FilePtr file(new File);
    file->setTitle(components.last());
    file->setParents(parentReferences);
    /*
    if (hasMetaData(QLatin1String("modified"))) {
        const QString modified = metaData(QLatin1String("modified"));
        kDebug() << modified;
        file->setModifiedDate(KDateTime::fromString(modified, KDateTime::ISODate));
    }
    */

    KTemporaryFile tmpFile;
    if (!readPutData(tmpFile)) {
        error(KIO::ERR_COULD_NOT_READ, url.path());
        return false;
    }

    FileCreateJob createJob(tmpFile.fileName(), file, getAccount(accountId));
    RUN_KGAPI_JOB_RETVAL(createJob, false)

    return true;
}


void KIOGDrive::put(const KUrl &url, int permissions, KIO::JobFlags flags)
{
    kDebug() << url << permissions << flags;

    const QString accountId = accountFromPath(url);
    const QStringList components = pathComponents(url);

    if (url.hasQueryItem(QLatin1String("id"))) {
        if (!putUpdate(url, accountId, components)) {
            return;
        }
    } else {
        if (!putCreate(url, accountId, components)) {
            return;
        }
    }

    // FIXME: Update the cache now!

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

    const QString sourceAccountId = accountFromPath(src);
    const QString destAccountId = accountFromPath(dest);

    // TODO: Does this actually happen, or does KIO treat our account name as host?
    if (sourceAccountId != destAccountId) {
        // KIO will fallback to get+post
        error(KIO::ERR_UNSUPPORTED_ACTION, src.path());
        return;
    }

    const QStringList srcPathComps = pathComponents(src);
    if (srcPathComps.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    } else if (srcPathComps.size() == 1) {
        error(KIO::ERR_ACCESS_DENIED, src.path());
        return;
    }

    const QString sourceFileId
        = src.hasQueryItem(QLatin1String("id"))
              ? src.queryItem(QLatin1String("id"))
              : resolveFileIdFromPath(src.path(KUrl::RemoveTrailingSlash));
    if (sourceFileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }
    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    sourceFileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::ModifiedDate |
                                 FileFetchJob::LastViewedByMeDate | FileFetchJob::Description);
    RUN_KGAPI_JOB_PARAMS(sourceFileFetchJob, src, sourceAccountId)

    const ObjectsList objects = sourceFileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }

    const FilePtr sourceFile = objects[0].dynamicCast<File>();

    const QStringList destPathComps = pathComponents(dest);
    ParentReferencesList destParentReferences;
    if (destPathComps.isEmpty()) {
        error(KIO::ERR_ACCESS_DENIED, dest.path());
        return;
    } else if (destPathComps.size() == 1) {
        // copy to root
    } else {
        const QString destDirId = destPathComps[destPathComps.count() - 2];
        destParentReferences << ParentReferencePtr(new ParentReference(destDirId));
    }
    const QString destFileName = destPathComps.last();

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

    const QString fileId
        = isfile && url.hasQueryItem(QLatin1String("id"))
            ? url.queryItem(QLatin1String("id"))
            : resolveFileIdFromPath(url.path(KUrl::RemoveTrailingSlash),
                                    isfile ? KIOGDrive::PathIsFile : KIOGDrive::PathIsFolder);
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }
    const QString accountId = accountFromPath(url);
    const QStringList components = pathComponents(url);

    // If user tries to delete the account folder, remove the account from KWallet
    if (components.count() == 1) {
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
            error(KIO::ERR_COULD_NOT_RMDIR, url.path());
            return;
        }
    }

    FileTrashJob trashJob(fileId, getAccount(accountId));
    RUN_KGAPI_JOB(trashJob)

    m_cache.removePath(url.path());

    finished();

}

void KIOGDrive::rename(const KUrl &src, const KUrl &dest, KIO::JobFlags flags)
{
    kDebug() << src << dest << flags;

    const QString sourceAccountId = accountFromPath(src);
    const QString destAccountId = accountFromPath(dest);

    // TODO: Does this actually happen, or does KIO treat our account name as host?
    if (sourceAccountId != destAccountId) {
        error(KIO::ERR_UNSUPPORTED_ACTION, src.path());
        return;
    }

    const QStringList srcPathComps = pathComponents(src);
    if (srcPathComps.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, dest.path());
        return;
    } else if (srcPathComps.size() == 1) {
        error(KIO::ERR_ACCESS_DENIED, dest.path());
        return;
    }
    const QString sourceFileId
        = src.hasQueryItem(QLatin1String("id"))
            ? src.queryItem(QLatin1String("id"))
            : resolveFileIdFromPath(src.path(KUrl::RemoveTrailingSlash),
                                    KIOGDrive::PathIsFile);
    if (sourceFileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }

    // We need to fetch ALL, so that we can do update later
    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    RUN_KGAPI_JOB_PARAMS(sourceFileFetchJob, src, sourceAccountId)

    const ObjectsList objects = sourceFileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }

    const FilePtr sourceFile = objects[0].dynamicCast<File>();

    ParentReferencesList parentReferences = sourceFile->parents();
    const QStringList destPathComps = pathComponents(dest);
    if (destPathComps.isEmpty()) {
        // paths.size == 0 -> error, user is trying to move to top-level gdrive:///
        error(KIO::ERR_ACCESS_DENIED, dest.fileName());
        return;
    } else if (destPathComps.size() == 1) {
        // user is trying to move to root -> we are only renaming
    } else {
        if (srcPathComps.size() < 3) {
            // WTF?
            error(KIO::ERR_DOES_NOT_EXIST, src.path());
            return;
        }

        // skip filename and extract the second-to-last component
        const QString destDirId = resolveFileIdFromPath(joinSublist(destPathComps, 0, destPathComps.count() - 2, QLatin1Char('/')),
                                                        KIOGDrive::PathIsFolder);
        const QString srcDirId = resolveFileIdFromPath(joinSublist(srcPathComps, 0, srcPathComps.count() - 2, QLatin1Char('/')),
                                                       KIOGDrive::PathIsFolder);

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
            error(KIO::ERR_DOES_NOT_EXIST, src.path());
            return;
        }

        // Add destination to parent references
        parentReferences << ParentReferencePtr(new ParentReference(destDirId));
    }

    const QString destFileName = destPathComps.last();

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

    const QString fileId
        = url.hasQueryItem(QLatin1String("id"))
            ? url.queryItem(QLatin1String("id"))
            : resolveFileIdFromPath(url.path(KUrl::RemoveTrailingSlash));
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }
    const QString accountId = accountFromPath(url);

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    fileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::MimeType);
    RUN_KGAPI_JOB(fileFetchJob)

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    const FilePtr file = objects.first().dynamicCast<File>();
    mimeType(file->mimeType());
    finished();
}
