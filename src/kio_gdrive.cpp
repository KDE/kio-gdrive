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
#include "gdrivebackend.h"
#include "gdrivedebug.h"
#include "gdrivehelper.h"
#include "gdriveurl.h"
#include "gdriveversion.h"

#include <QApplication>
#include <QUrlQuery>
#include <QTemporaryFile>

#include <KGAPI/Account>
#include <KGAPI/AuthJob>
#include <KGAPI/Drive/About>
#include <KGAPI/Drive/AboutFetchJob>
#include <KGAPI/Drive/ChildReference>
#include <KGAPI/Drive/ChildReferenceFetchJob>
#include <KGAPI/Drive/ChildReferenceCreateJob>
#include <KGAPI/Drive/File>
#include <KGAPI/Drive/FileCopyJob>
#include <KGAPI/Drive/FileCreateJob>
#include <KGAPI/Drive/FileModifyJob>
#include <KGAPI/Drive/FileTrashJob>
#include <KGAPI/Drive/FileFetchJob>
#include <KGAPI/Drive/FileFetchContentJob>
#include <KGAPI/Drive/FileSearchQuery>
#include <KGAPI/Drive/ParentReference>
#include <KGAPI/Drive/Permission>
#include <KIO/AccessManager>
#include <KIO/Job>
#include <KLocalizedString>

#include <QNetworkRequest>
#include <QNetworkReply>

using namespace KGAPI2;
using namespace Drive;

class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.slave.gdrive" FILE "gdrive.json")
};

extern "C"
{
    int Q_DECL_EXPORT kdemain(int argc, char **argv)
    {
        QApplication app(argc, argv);
        app.setApplicationName(QStringLiteral("kio_gdrive"));

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

    m_accountManager.reset(new AccountManager);

    qCDebug(GDRIVE) << "KIO GDrive ready: version" << GDRIVE_VERSION_STRING;
}

KIOGDrive::~KIOGDrive()
{
    closeConnection();
}

KIOGDrive::Action KIOGDrive::handleError(const KGAPI2::Job &job, const QUrl &url)
{
    qCDebug(GDRIVE) << "Job status code:" << job.error() << "- message:" << job.errorString();

    switch (job.error()) {
        case KGAPI2::OK:
        case KGAPI2::NoError:
            return Success;
        case KGAPI2::AuthCancelled:
        case KGAPI2::AuthError:
            error(KIO::ERR_CANNOT_LOGIN, url.toDisplayString());
            return Fail;
        case KGAPI2::Unauthorized: {
            const AccountPtr oldAccount = job.account();
            const AccountPtr account = m_accountManager->refreshAccount(oldAccount);
            if (!account) {
                error(KIO::ERR_CANNOT_LOGIN, url.toDisplayString());
                return Fail;
            }
            return Restart;
        }
        case KGAPI2::Forbidden:
            error(KIO::ERR_ACCESS_DENIED, url.toDisplayString());
            return Fail;
        case KGAPI2::NotFound:
            error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
            return Fail;
        case KGAPI2::NoContent:
            error(KIO::ERR_NO_CONTENT, url.toDisplayString());
            return Fail;
        case KGAPI2::QuotaExceeded:
            error(KIO::ERR_DISK_FULL, url.toDisplayString());
            return Fail;
        default:
            error(KIO::ERR_SLAVE_DEFINED, job.errorString());
            return Fail;
    }

    return Fail;
}

void KIOGDrive::fileSystemFreeSpace(const QUrl &url)
{
    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();
    if (accountId == QLatin1String("new-account")) {
        finished();
        return;
    }
    if (!gdriveUrl.isRoot()) {
        AboutFetchJob aboutFetch(getAccount(accountId));
        if (runJob(aboutFetch, url, accountId)) {
            const AboutPtr about = aboutFetch.aboutData();
            if (about) {
                setMetaData(QStringLiteral("total"), QString::number(about->quotaBytesTotal()));
                setMetaData(QStringLiteral("available"), QString::number(about->quotaBytesTotal() - about->quotaBytesUsedAggregate()));
                finished();
                return;
            }
        }
    }
    error(KIO::ERR_CANNOT_STAT, url.toDisplayString());
}

AccountPtr KIOGDrive::getAccount(const QString &accountName)
{
    return m_accountManager->account(accountName);
}

void KIOGDrive::virtual_hook(int id, void *data)
{
    switch (id) {
        case SlaveBase::GetFileSystemFreeSpace: {
            QUrl *url = static_cast<QUrl *>(data);
            fileSystemFreeSpace(*url);
            break;
        }
        default:
            SlaveBase::virtual_hook(id, data);
    }
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
        entry.insert(KIO::UDSEntry::UDS_URL, QStringLiteral("gdrive://%1/%2?id=%3").arg(path, origFile->title(), origFile->id()));
    }

    entry.insert(KIO::UDSEntry::UDS_CREATION_TIME, file->createdDate().toTime_t());
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, file->modifiedDate().toTime_t());
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, file->lastViewedByMeDate().toTime_t());
    if (!file->ownerNames().isEmpty()) {
        entry.insert(KIO::UDSEntry::UDS_USER, file->ownerNames().first());
    }

    if (!isFolder) {
        if (file->editable()) {
            entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        } else {
            entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH);
        }
    } else {
        entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }

    return entry;
}

void KIOGDrive::openConnection()
{
    qCDebug(GDRIVE) << "Ready to talk to GDrive";
}

KIO::UDSEntry KIOGDrive::accountToUDSEntry(const QString &accountNAme)
{
    KIO::UDSEntry entry;

    entry.insert(KIO::UDSEntry::UDS_NAME, accountNAme);
    entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, accountNAme);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    entry.insert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-gdrive"));

    return entry;
}

void KIOGDrive::createAccount()
{
    const KGAPI2::AccountPtr account = m_accountManager->createAccount();
    if (!account->accountName().isEmpty()) {
        // Redirect to the account we just created.
        redirection(QUrl(QStringLiteral("gdrive:/%1").arg(account->accountName())));
        finished();
        return;
    }

    if (m_accountManager->accounts().isEmpty()) {
        error(KIO::ERR_SLAVE_DEFINED, i18n("There are no Google Drive accounts enabled. Please add at least one."));
        return;
    }

    // Redirect to the root, we already have some account.
    redirection(QUrl(QStringLiteral("gdrive:/")));
    finished();
}

void KIOGDrive::listAccounts()
{
    const auto accounts = m_accountManager->accounts();
    if (accounts.isEmpty()) {
        createAccount();
        return;
    }

    Q_FOREACH (const QString &account, accounts) {
        const KIO::UDSEntry entry = accountToUDSEntry(account);
        listEntry(entry);
    }
    KIO::UDSEntry newAccountEntry;
    newAccountEntry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("new-account"));
    newAccountEntry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18nc("login in a new google account", "New account"));
    newAccountEntry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    newAccountEntry.insert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("list-add-user"));
    listEntry(newAccountEntry);

    // Create also non-writable UDSentry for "."
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    listEntry(entry);

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

    RecursionDepthCounter(const RecursionDepthCounter &) = delete;
    RecursionDepthCounter& operator=(const RecursionDepthCounter &) = delete;

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
    qCDebug(GDRIVE) << Q_FUNC_INFO << path;

    if (path.isEmpty()) {
        return QString();
    }

    QString fileId = m_cache.idForPath(path);
    if (!fileId.isEmpty()) {
        qCDebug(GDRIVE) << "Resolved" << path << "to" << fileId << "(from cache)";
        return fileId;
    }

    QUrl url;
    url.setScheme(QStringLiteral("gdrive"));
    url.setPath(path);
    const auto gdriveUrl = GDriveUrl(url);
    Q_ASSERT(!gdriveUrl.isRoot());

    const QStringList components = gdriveUrl.pathComponents();
    if (gdriveUrl.isAccountRoot() || (components.size() == 2 && components[1] == QLatin1String("trash"))) {
        qCDebug(GDRIVE) << "Resolved" << path << "to \"root\"";
        return rootFolderId(components[0]);
    }

    // Try to recursively resolve ID of parent path - either from cache, or by
    // querying Google
    const QString parentId = resolveFileIdFromPath(gdriveUrl.parentPath(), KIOGDrive::PathIsFolder);
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

    const QString accountId = gdriveUrl.account();
    FileFetchJob fetchJob(query, getAccount(accountId));
    fetchJob.setFields(FileFetchJob::Id | FileFetchJob::Title | FileFetchJob::Labels);
    if (!runJob(fetchJob, url, accountId)) {
        return QString();
    }

    const ObjectsList objects = fetchJob.items();
    qCDebug(GDRIVE) << objects;
    if (objects.count() == 0) {
        qCWarning(GDRIVE) << "Failed to resolve" << path;
        return QString();
    }

    const FilePtr file = objects[0].dynamicCast<File>();

    m_cache.insertPath(path, file->id());

    qCDebug(GDRIVE) << "Resolved" << path << "to" << file->id() << "(from network)";
    return file->id();
}

QString KIOGDrive::rootFolderId(const QString &accountId)
{
    auto it = m_rootIds.constFind(accountId);
    if (it == m_rootIds.cend()) {
        AboutFetchJob aboutFetch(getAccount(accountId));
        QUrl url;
        if (!runJob(aboutFetch, url, accountId)) {
            return QString();
        }

        const AboutPtr about = aboutFetch.aboutData();
        if (!about || about->rootFolderId().isEmpty()) {
            qCWarning(GDRIVE) << "Failed to obtain root ID";
            return QString();
        }

        auto v = m_rootIds.insert(accountId, about->rootFolderId());
        return *v;
    }

    return *it;
}

void KIOGDrive::listDir(const QUrl &url)
{
    qCDebug(GDRIVE) << "Going to list" << url;

    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();
    if (accountId == QLatin1String("new-account")) {
        createAccount();
        return;
    }

    QString folderId;
    if (gdriveUrl.isRoot())  {
        listAccounts();
        return;
    } else if (gdriveUrl.isAccountRoot()) {
        folderId = rootFolderId(accountId);
    } else {
        folderId = m_cache.idForPath(url.path());
        if (folderId.isEmpty()) {
            folderId = resolveFileIdFromPath(url.adjusted(QUrl::StripTrailingSlash).path(),
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
    runJob(fileFetchJob, url, accountId);

    ObjectsList objects = fileFetchJob.items();
    Q_FOREACH (const ObjectPtr &object, objects) {
        const FilePtr file = object.dynamicCast<File>();

        const KIO::UDSEntry entry = fileToUDSEntry(file, url.adjusted(QUrl::StripTrailingSlash).path());
        listEntry(entry);

        const QString path = url.path().endsWith(QLatin1Char('/')) ? url.path() : url.path() + QLatin1Char('/');
        m_cache.insertPath(path + file->title(), file->id());
    }

    // We also need a non-null and writable UDSentry for "."
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    listEntry(entry);

    finished();
}


void KIOGDrive::mkdir(const QUrl &url, int permissions)
{
    // NOTE: We deliberately ignore the permissions field here, because GDrive
    // does not recognize any privileges that could be mapped to standard UNIX
    // file permissions.
    Q_UNUSED(permissions);

    qCDebug(GDRIVE) << "Creating directory" << url;

    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();
    // At least account and new folder name
    if (gdriveUrl.isRoot() || gdriveUrl.isAccountRoot()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }
    QString parentId;
    const auto components = gdriveUrl.pathComponents();
    if (components.size() == 2) {
        parentId = rootFolderId(accountId);
    } else {
        parentId = resolveFileIdFromPath(gdriveUrl.parentPath(), KIOGDrive::PathIsFolder);
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
    runJob(createJob, url, accountId);

    finished();
}

void KIOGDrive::stat(const QUrl &url)
{
    qCDebug(GDRIVE) << "Going to stat()" << url;

    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();
    if (gdriveUrl.isRoot()) {
        // TODO Can we stat() root?
        finished();
        return;
    }
    if (gdriveUrl.isAccountRoot()) {
        const KIO::UDSEntry entry = accountToUDSEntry(accountId);
        statEntry(entry);
        finished();
        return;
    }

    const QUrlQuery urlQuery(url);
    const QString fileId
        = urlQuery.hasQueryItem(QStringLiteral("id"))
            ? urlQuery.queryItemValue(QStringLiteral("id"))
            : resolveFileIdFromPath(url.adjusted(QUrl::StripTrailingSlash).path(),
                                    KIOGDrive::None);
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    runJob(fileFetchJob, url, accountId);

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

    const KIO::UDSEntry entry = fileToUDSEntry(file, gdriveUrl.parentPath());

    statEntry(entry);
    finished();
}

void KIOGDrive::get(const QUrl &url)
{
    qCDebug(GDRIVE) << "Fetching content of" << url;

    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();

    if (gdriveUrl.isRoot()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }
    if (gdriveUrl.isAccountRoot()) {
        // You cannot GET an account folder!
        error(KIO::ERR_ACCESS_DENIED, url.path());
        return;
    }

    const QUrlQuery urlQuery(url);
    const QString fileId =
        urlQuery.hasQueryItem(QStringLiteral("id"))
            ? urlQuery.queryItemValue(QStringLiteral("id"))
            : resolveFileIdFromPath(url.adjusted(QUrl::StripTrailingSlash).path(),
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
    runJob(fileFetchJob, url, accountId);

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
    runJob(contentJob, url, accountId);

    data(contentJob.data());
    finished();
}

bool KIOGDrive::readPutData(QTemporaryFile &tempFile)
{
    // TODO: Instead of using a temp file, upload directly the raw data (requires
    // support in LibKGAPI)

    // TODO: For large files, switch to resumable upload and upload the file in
    // reasonably large chunks (requires support in LibKGAPI)

    // TODO: Support resumable upload (requires support in LibKGAPI)

    if (!tempFile.open()) {
        error(KIO::ERR_CANNOT_WRITE, tempFile.fileName());
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
                error(KIO::ERR_CANNOT_WRITE, tempFile.fileName());
                return false;
            }
        }
    } while (result > 0);
    tempFile.close();

    if (result == -1) {
        qCWarning(GDRIVE) << "Could not read source file" << tempFile.fileName();
        error(KIO::ERR_CANNOT_READ, QString());
        return false;
    }

    return true;
}

bool KIOGDrive::runJob(KGAPI2::Job &job, const QUrl &url, const QString &accountId)
{
    KIOGDrive::Action action = KIOGDrive::Fail;
    Q_FOREVER {
        qCDebug(GDRIVE) << "Running job" << (&job) << "with accessToken" << job.account()->accessToken();
        QEventLoop eventLoop;
        QObject::connect(&job, &KGAPI2::Job::finished,
                         &eventLoop, &QEventLoop::quit);
        eventLoop.exec();
        action = handleError(job, url);
        if (action == KIOGDrive::Success) {
            break;
        } else if (action == KIOGDrive::Fail) {
            return false;
        }
        job.setAccount(getAccount(accountId));
        job.restart();
    };

    return true;
}

bool KIOGDrive::putUpdate(const QUrl &url)
{
    const QString fileId = QUrlQuery(url).queryItemValue(QStringLiteral("id"));
    qCDebug(GDRIVE) << Q_FUNC_INFO << url << fileId;

    const auto gdriveUrl = GDriveUrl(url);
    const auto accountId = gdriveUrl.account();

    FileFetchJob fetchJob(fileId, getAccount(accountId));
    if (!runJob(fetchJob, url, accountId)) {
        return false;
    }

    const ObjectsList objects = fetchJob.items();
    if (objects.size() != 1) {
        putCreate(url);
        return false;
    }

    const FilePtr file = objects[0].dynamicCast<File>();
    QTemporaryFile tmpFile;
    if (!readPutData(tmpFile)) {
        error(KIO::ERR_CANNOT_READ, url.path());
        return false;
    }

    FileModifyJob modifyJob(tmpFile.fileName(), file, getAccount(accountId));
    modifyJob.setUpdateModifiedDate(true);
    if (!runJob(modifyJob, url, accountId)) {
        return false;
    }

    return true;
}

bool KIOGDrive::putCreate(const QUrl &url)
{
    qCDebug(GDRIVE) << Q_FUNC_INFO << url;
    ParentReferencesList parentReferences;

    const auto gdriveUrl = GDriveUrl(url);
    if (gdriveUrl.isRoot() || gdriveUrl.isAccountRoot()) {
        error(KIO::ERR_ACCESS_DENIED, url.path());
        return false;
    }
    const auto components = gdriveUrl.pathComponents();
    if (components.length() == 2) {
        // Creating in root directory
    } else {
        const QString parentId = resolveFileIdFromPath(gdriveUrl.parentPath());
        if (parentId.isEmpty()) {
            error(KIO::ERR_DOES_NOT_EXIST, url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).path());
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
        qCDebug(GDRIVE) << modified;
        file->setModifiedDate(KDateTime::fromString(modified, KDateTime::ISODate));
    }
    */

    QTemporaryFile tmpFile;
    if (!readPutData(tmpFile)) {
        error(KIO::ERR_CANNOT_READ, url.path());
        return false;
    }

    const auto accountId = gdriveUrl.account();
    FileCreateJob createJob(tmpFile.fileName(), file, getAccount(accountId));
    if (!runJob(createJob, url, accountId)) {
        return false;
    }

    return true;
}


void KIOGDrive::put(const QUrl &url, int permissions, KIO::JobFlags flags)
{
    // NOTE: We deliberately ignore the permissions field here, because GDrive
    // does not recognize any privileges that could be mapped to standard UNIX
    // file permissions.
    Q_UNUSED(permissions)
    Q_UNUSED(flags)

    qCDebug(GDRIVE) << Q_FUNC_INFO << url;

    if (QUrlQuery(url).hasQueryItem(QStringLiteral("id"))) {
        if (!putUpdate(url)) {
            return;
        }
    } else {
        if (!putCreate(url)) {
            return;
        }
    }

    // FIXME: Update the cache now!

    finished();
}


void KIOGDrive::copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags)
{
    qCDebug(GDRIVE) << "Going to copy" << src << "to" << dest;

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

    const auto srcGDriveUrl = GDriveUrl(src);
    const auto destGDriveUrl = GDriveUrl(dest);
    const QString sourceAccountId = srcGDriveUrl.account();
    const QString destAccountId = destGDriveUrl.account();

    // TODO: Does this actually happen, or does KIO treat our account name as host?
    if (sourceAccountId != destAccountId) {
        // KIO will fallback to get+post
        error(KIO::ERR_UNSUPPORTED_ACTION, src.path());
        return;
    }

    if (srcGDriveUrl.isRoot()) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }
    if (srcGDriveUrl.isAccountRoot()) {
        error(KIO::ERR_ACCESS_DENIED, src.path());
        return;
    }

    const QUrlQuery urlQuery(src);
    const QString sourceFileId
        = urlQuery.hasQueryItem(QStringLiteral("id"))
              ? urlQuery.queryItemValue(QStringLiteral("id"))
              : resolveFileIdFromPath(src.adjusted(QUrl::StripTrailingSlash).path());
    if (sourceFileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }
    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    sourceFileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::ModifiedDate |
                                 FileFetchJob::LastViewedByMeDate | FileFetchJob::Description);
    runJob(sourceFileFetchJob, src, sourceAccountId);

    const ObjectsList objects = sourceFileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }

    const FilePtr sourceFile = objects[0].dynamicCast<File>();

    ParentReferencesList destParentReferences;
    if (destGDriveUrl.isRoot()) {
        error(KIO::ERR_ACCESS_DENIED, dest.path());
        return;
    }

    QString destDirId;
    const auto destPathComps = destGDriveUrl.pathComponents();
    const QString destFileName = destPathComps.last();
    if (destPathComps.size() == 2) {
        destDirId = rootFolderId(destAccountId);
    } else {
        destDirId = resolveFileIdFromPath(destGDriveUrl.parentPath(), KIOGDrive::PathIsFolder);
    }
    destParentReferences << ParentReferencePtr(new ParentReference(destDirId));

    FilePtr destFile(new File);
    destFile->setTitle(destFileName);
    destFile->setModifiedDate(sourceFile->modifiedDate());
    destFile->setLastViewedByMeDate(sourceFile->lastViewedByMeDate());
    destFile->setDescription(sourceFile->description());
    destFile->setParents(destParentReferences);

    FileCopyJob copyJob(sourceFile, destFile, getAccount(sourceAccountId));
    runJob(copyJob, dest, sourceAccountId);

    finished();
}

void KIOGDrive::del(const QUrl &url, bool isfile)
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

    qCDebug(GDRIVE) << "Deleting URL" << url << "- is it a file?" << isfile;

    const QUrlQuery urlQuery(url);
    const QString fileId
        = isfile && urlQuery.hasQueryItem(QStringLiteral("id"))
            ? urlQuery.queryItemValue(QStringLiteral("id"))
            : resolveFileIdFromPath(url.adjusted(QUrl::StripTrailingSlash).path(),
                                    isfile ? KIOGDrive::PathIsFile : KIOGDrive::PathIsFolder);
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }
    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();

    // If user tries to delete the account folder, remove the account from the keychain
    if (gdriveUrl.isAccountRoot()) {
        const KGAPI2::AccountPtr account = getAccount(accountId);
        if (account->accountName().isEmpty()) {
            error(KIO::ERR_DOES_NOT_EXIST, accountId);
            return;
        }
        m_accountManager->removeAccount(accountId);
        finished();
        return;
    }

    // GDrive allows us to delete entire directory even when it's not empty,
    // so we need to emulate the normal behavior ourselves by checking number of
    // child references
    if (!isfile) {
        ChildReferenceFetchJob referencesFetch(fileId, getAccount(accountId));
        runJob(referencesFetch, url, accountId);
        const bool isEmpty = !referencesFetch.items().count();

        if (!isEmpty && metaData(QStringLiteral("recurse")) != QLatin1String("true")) {
            error(KIO::ERR_CANNOT_RMDIR, url.path());
            return;
        }
    }

    FileTrashJob trashJob(fileId, getAccount(accountId));
    runJob(trashJob, url, accountId);

    m_cache.removePath(url.path());

    finished();

}

void KIOGDrive::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags)
{
    Q_UNUSED(flags)
    qCDebug(GDRIVE) << "Renaming" << src << "to" << dest;

    const auto srcGDriveUrl = GDriveUrl(src);
    const auto destGDriveUrl = GDriveUrl(dest);
    const QString sourceAccountId = srcGDriveUrl.account();
    const QString destAccountId = destGDriveUrl.account();

    // TODO: Does this actually happen, or does KIO treat our account name as host?
    if (sourceAccountId != destAccountId) {
        error(KIO::ERR_UNSUPPORTED_ACTION, src.path());
        return;
    }

    if (srcGDriveUrl.isRoot()) {
        error(KIO::ERR_DOES_NOT_EXIST, dest.path());
        return;
    }
    if (srcGDriveUrl.isAccountRoot()) {
        error(KIO::ERR_ACCESS_DENIED, dest.path());
        return;
    }

    const QUrlQuery urlQuery(src);
    const QString sourceFileId
        = urlQuery.hasQueryItem(QStringLiteral("id"))
            ? urlQuery.queryItemValue(QStringLiteral("id"))
            : resolveFileIdFromPath(src.adjusted(QUrl::StripTrailingSlash).path(),
                                    KIOGDrive::PathIsFile);
    if (sourceFileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }

    // We need to fetch ALL, so that we can do update later
    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    runJob(sourceFileFetchJob, src, sourceAccountId);

    const ObjectsList objects = sourceFileFetchJob.items();
    if (objects.count() != 1) {
        qCDebug(GDRIVE) << "FileFetchJob retrieved" << objects.count() << "items, while only one was expected.";
        error(KIO::ERR_DOES_NOT_EXIST, src.path());
        return;
    }

    const FilePtr sourceFile = objects[0].dynamicCast<File>();

    ParentReferencesList parentReferences = sourceFile->parents();
    if (destGDriveUrl.isRoot()) {
        // user is trying to move to top-level gdrive:///
        error(KIO::ERR_ACCESS_DENIED, dest.fileName());
        return;
    }
    const auto srcPathComps = srcGDriveUrl.pathComponents();
    const auto destPathComps = destGDriveUrl.pathComponents();
    if (destGDriveUrl.isAccountRoot()) {
        // user is trying to move to root -> we are only renaming
    } else {
         // skip filename and extract the second-to-last component
        const QString destDirId = resolveFileIdFromPath(destGDriveUrl.parentPath(), KIOGDrive::PathIsFolder);
        const QString srcDirId = resolveFileIdFromPath(srcGDriveUrl.parentPath(), KIOGDrive::PathIsFolder);

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
            qCDebug(GDRIVE) << "Could not remove" << src << "from parent references.";
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
    runJob(modifyJob, dest, sourceAccountId);

    finished();
}

void KIOGDrive::mimetype(const QUrl &url)
{
    qCDebug(GDRIVE) << Q_FUNC_INFO << url;

    const QUrlQuery urlQuery(url);
    const QString fileId
        = urlQuery.hasQueryItem(QStringLiteral("id"))
            ? urlQuery.queryItemValue(QStringLiteral("id"))
            : resolveFileIdFromPath(url.adjusted(QUrl::StripTrailingSlash).path());
    if (fileId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }
    const QString accountId = GDriveUrl(url).account();

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    fileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::MimeType);
    runJob(fileFetchJob, url, accountId);

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    const FilePtr file = objects.first().dynamicCast<File>();
    mimeType(file->mimeType());
    finished();
}

#include "kio_gdrive.moc"
