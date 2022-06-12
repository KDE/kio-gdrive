/*
 * SPDX-FileCopyrightText: 2013-2014 Daniel Vrátil <dvratil@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "kio_gdrive.h"
#include "gdrivebackend.h"
#include "gdrivedebug.h"
#include "gdrivehelper.h"
#include "gdriveurl.h"
#include "gdriveversion.h"
#include "gdrive_udsentry.h"

#include <QApplication>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrlQuery>
#include <QUuid>

#include <KGAPI/Account>
#include <KGAPI/AuthJob>
#include <KGAPI/Drive/About>
#include <KGAPI/Drive/AboutFetchJob>
#include <KGAPI/Drive/ChildReference>
#include <KGAPI/Drive/ChildReferenceCreateJob>
#include <KGAPI/Drive/ChildReferenceFetchJob>
#include <KGAPI/Drive/Drives>
#include <KGAPI/Drive/DrivesCreateJob>
#include <KGAPI/Drive/DrivesDeleteJob>
#include <KGAPI/Drive/DrivesFetchJob>
#include <KGAPI/Drive/DrivesModifyJob>
#include <KGAPI/Drive/File>
#include <KGAPI/Drive/FileCopyJob>
#include <KGAPI/Drive/FileCreateJob>
#include <KGAPI/Drive/FileFetchContentJob>
#include <KGAPI/Drive/FileFetchJob>
#include <KGAPI/Drive/FileModifyJob>
#include <KGAPI/Drive/FileSearchQuery>
#include <KGAPI/Drive/FileTrashJob>
#include <KGAPI/Drive/ParentReference>
#include <KGAPI/Drive/Permission>
#include <KIO/AccessManager>
#include <KIO/Job>
#include <KLocalizedString>

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
    qCDebug(GDRIVE) << "Completed job" << (&job) << "error code:" << job.error() << "- message:" << job.errorString();

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
    if (gdriveUrl.isNewAccountPath()) {
        qCDebug(GDRIVE) << "fileSystemFreeSpace is not supported for new-account url";
        finished();
        return;
    }
    if (gdriveUrl.isRoot()) {
        qCDebug(GDRIVE) << "fileSystemFreeSpace is not supported for gdrive root url";
        error(KIO::ERR_CANNOT_STAT, url.toDisplayString());
        return;
    }

    qCDebug(GDRIVE) << "Getting fileSystemFreeSpace for" << url;
    const QString accountId = gdriveUrl.account();
    AboutFetchJob aboutFetch(getAccount(accountId));
    aboutFetch.setFields({
        About::Fields::Kind,
        About::Fields::QuotaBytesTotal,
        About::Fields::QuotaBytesUsedAggregate
    });
    if (runJob(aboutFetch, url, accountId)) {
        const AboutPtr about = aboutFetch.aboutData();
        if (about) {
            setMetaData(QStringLiteral("total"), QString::number(about->quotaBytesTotal()));
            setMetaData(QStringLiteral("available"), QString::number(about->quotaBytesTotal() - about->quotaBytesUsedAggregate()));
            finished();
        }
    }
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

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, file->title());
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, file->title());
    entry.fastInsert(KIO::UDSEntry::UDS_COMMENT, file->description());

    if (file->isFolder()) {
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
        isFolder = true;
    } else {
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, file->mimeType());
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, file->fileSize());

        entry.fastInsert(KIO::UDSEntry::UDS_URL, fileToUrl(origFile, path).toString());
    }

    entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, file->createdDate().toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, file->modifiedDate().toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, file->lastViewedByMeDate().toSecsSinceEpoch());
    if (!file->ownerNames().isEmpty()) {
        entry.fastInsert(KIO::UDSEntry::UDS_USER, file->ownerNames().first());
    }

    if (!isFolder) {
        if (file->editable()) {
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        } else {
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH);
        }
    } else {
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }

    entry.fastInsert(GDriveUDSEntryExtras::Id, file->id());
    entry.fastInsert(GDriveUDSEntryExtras::Url, file->alternateLink().toString());
    entry.fastInsert(GDriveUDSEntryExtras::Version, QString::number(file->version()));
    entry.fastInsert(GDriveUDSEntryExtras::Md5, file->md5Checksum());
    entry.fastInsert(GDriveUDSEntryExtras::LastModifyingUser, file->lastModifyingUserName());
    entry.fastInsert(GDriveUDSEntryExtras::Owners, file->ownerNames().join(QStringLiteral(", ")));

    return entry;
}

QUrl KIOGDrive::fileToUrl(const FilePtr &file, const QString &path) const
{
    QUrl url;
    url.setScheme(GDriveUrl::Scheme);
    url.setPath(path + QLatin1Char('/') + file->title());

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("id"), file->id());
    url.setQuery(urlQuery);

    return url;
}

void KIOGDrive::openConnection()
{
    qCDebug(GDRIVE) << "Ready to talk to GDrive";
}

KIO::UDSEntry KIOGDrive::newAccountUDSEntry()
{
    KIO::UDSEntry entry;

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, GDriveUrl::NewAccountPath);
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18nc("login in a new google account", "New account"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("list-add-user"));
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR);

    return entry;
}

KIO::UDSEntry KIOGDrive::accountToUDSEntry(const QString &accountNAme)
{
    KIO::UDSEntry entry;

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, accountNAme);
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, accountNAme);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-gdrive"));

    return entry;
}

KIO::UDSEntry KIOGDrive::sharedDriveToUDSEntry(const DrivesPtr &sharedDrive)
{
    KIO::UDSEntry entry;

    qlonglong udsAccess = S_IRUSR | S_IXUSR | S_IRGRP;
    if (sharedDrive->capabilities()->canRenameDrive() || sharedDrive->capabilities()->canDeleteDrive()) {
        udsAccess |= S_IWUSR;
    }

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, sharedDrive->id());
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, sharedDrive->name());
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, sharedDrive->createdDate().toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, udsAccess);
    entry.fastInsert(KIO::UDSEntry::UDS_HIDDEN, sharedDrive->hidden());
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-gdrive"));

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

    for (const QString &account : accounts) {
        const KIO::UDSEntry entry = accountToUDSEntry(account);
        listEntry(entry);
    }

    KIO::UDSEntry newAccountEntry = newAccountUDSEntry();
    listEntry(newAccountEntry);

    // Create also non-writable UDSentry for "."
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    listEntry(entry);

    finished();
}

void KIOGDrive::listSharedDrivesRoot(const QUrl &url)
{
    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();
    DrivesFetchJob sharedDrivesFetchJob(getAccount(accountId));
    sharedDrivesFetchJob.setFields({
        Drives::Fields::Kind,
        Drives::Fields::Id,
        Drives::Fields::Name,
        Drives::Fields::Hidden,
        Drives::Fields::CreatedDate,
        Drives::Fields::Capabilities
    });

    if (runJob(sharedDrivesFetchJob, url, accountId)) {
        const auto objects = sharedDrivesFetchJob.items();
        for (const auto &object : objects) {
            const DrivesPtr sharedDrive = object.dynamicCast<Drives>();
            const KIO::UDSEntry entry = sharedDriveToUDSEntry(sharedDrive);
            listEntry(entry);
        }

        auto entry = fetchSharedDrivesRootEntry(accountId, FetchEntryFlags::CurrentDir);
        listEntry(entry);

        finished();
    }
}

bool KIOGDrive::createSharedDrive(const QUrl &url)
{
    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();

    DrivesPtr sharedDrive = DrivesPtr::create();
    sharedDrive->setName(gdriveUrl.filename());

    const QString requestId = QUuid::createUuid().toString();
    DrivesCreateJob createJob(requestId, sharedDrive, getAccount(accountId));
    return runJob(createJob, url, accountId);
}

bool KIOGDrive::deleteSharedDrive(const QUrl &url)
{
    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();
    DrivesDeleteJob sharedDriveDeleteJob(gdriveUrl.filename(), getAccount(accountId));
    return runJob(sharedDriveDeleteJob, url, accountId);
}

void KIOGDrive::statSharedDrive(const QUrl &url)
{
    const auto gdriveUrl = GDriveUrl(url);
    const QString accountId = gdriveUrl.account();

    const auto sharedDriveId = resolveSharedDriveId(gdriveUrl.filename(), accountId);
    if (sharedDriveId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    DrivesFetchJob sharedDriveFetchJob(sharedDriveId, getAccount(accountId));
    sharedDriveFetchJob.setFields({
        Drives::Fields::Kind,
        Drives::Fields::Id,
        Drives::Fields::Name,
        Drives::Fields::Hidden,
        Drives::Fields::CreatedDate,
        Drives::Fields::Capabilities
    });
    if (!runJob(sharedDriveFetchJob, url, accountId)) {
        return;
    }

    ObjectPtr object = sharedDriveFetchJob.items().at(0);
    const DrivesPtr sharedDrive = object.dynamicCast<Drives>();
    const auto entry = sharedDriveToUDSEntry(sharedDrive);
    statEntry(entry);
}

KIO::UDSEntry KIOGDrive::fetchSharedDrivesRootEntry(const QString &accountId, FetchEntryFlags flags)
{
    // Not every user is allowed to create shared Drives,
    // check with About resource.
    bool canCreateDrives = false;
    AboutFetchJob aboutFetch(getAccount(accountId));
    aboutFetch.setFields({
        About::Fields::Kind,
        About::Fields::CanCreateDrives
    });
    QEventLoop eventLoop;
    QObject::connect(&aboutFetch, &KGAPI2::Job::finished,
                     &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    if (aboutFetch.error() == KGAPI2::OK || aboutFetch.error() == KGAPI2::NoError) {
        const AboutPtr about = aboutFetch.aboutData();
        if (about) {
            canCreateDrives = about->canCreateDrives();
        }
    }
    qCDebug(GDRIVE) << "Account" << accountId << (canCreateDrives ? "can" : "can't") << "create Drives";

    KIO::UDSEntry entry;

    if (flags == FetchEntryFlags::CurrentDir) {
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    } else {
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, GDriveUrl::SharedDrivesDir);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("Shared Drives"));
    }
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-gdrive"));

    qlonglong udsAccess = S_IRUSR | S_IXUSR;
    // If user is allowed to create shared Drives, add write bit on directory
    if (canCreateDrives) {
        udsAccess |= S_IWUSR;
    }
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, udsAccess);

    return entry;
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
    qCDebug(GDRIVE) << "Resolving file ID for" << path;

    if (path.isEmpty()) {
        return QString();
    }

    const QString fileId = m_cache.idForPath(path);
    if (!fileId.isEmpty()) {
        qCDebug(GDRIVE) << "Resolved" << path << "to" << fileId << "(from cache)";
        return fileId;
    }

    QUrl url;
    url.setScheme(GDriveUrl::Scheme);
    url.setPath(path);
    const auto gdriveUrl = GDriveUrl(url);
    Q_ASSERT(!gdriveUrl.isRoot());

    if (gdriveUrl.isAccountRoot() || gdriveUrl.isTrashDir()) {
        qCDebug(GDRIVE) << "Resolved" << path << "to account root";
        return rootFolderId(gdriveUrl.account());
    }

    if (gdriveUrl.isSharedDrive()) {
        // The gdriveUrl.filename() could be the Shared Drive id or
        // the name depending on whether we are navigating from a parent
        // or accessing the url directly, use the shared drive specific
        // solver to disambiguate
        return resolveSharedDriveId(gdriveUrl.filename(), gdriveUrl.account());
    }

    if (gdriveUrl.isSharedDrivesRoot()) {
        qCDebug(GDRIVE) << "Resolved" << path << "to Shared Drives root";
        return QString();
    }

    // Try to recursively resolve ID of parent path - either from cache, or by
    // querying Google
    const QString parentId = resolveFileIdFromPath(gdriveUrl.parentPath(), KIOGDrive::PathIsFolder);
    if (parentId.isEmpty()) {
        // We failed to resolve parent -> error
        return QString();
    }

    qCDebug(GDRIVE) << "Getting ID for" << gdriveUrl.filename() << "in parent with ID" << parentId;

    FileSearchQuery query;
    if (flags != KIOGDrive::None) {
        query.addQuery(FileSearchQuery::MimeType,
                       (flags & KIOGDrive::PathIsFolder ? FileSearchQuery::Equals : FileSearchQuery::NotEquals),
                       GDriveHelper::folderMimeType());
    }
    query.addQuery(FileSearchQuery::Title, FileSearchQuery::Equals, gdriveUrl.filename());
    query.addQuery(FileSearchQuery::Parents, FileSearchQuery::In, parentId);
    query.addQuery(FileSearchQuery::Trashed, FileSearchQuery::Equals, gdriveUrl.isTrashed());

    const QString accountId = gdriveUrl.account();
    FileFetchJob fetchJob(query, getAccount(accountId));
    fetchJob.setFields({File::Fields::Id, File::Fields::Title, File::Fields::Labels});
    if (!runJob(fetchJob, url, accountId)) {
        return QString();
    }

    const ObjectsList objects = fetchJob.items();
    if (objects.isEmpty()) {
        qCWarning(GDRIVE) << "Failed to resolve" << path;
        return QString();
    }

    const FilePtr file = objects[0].dynamicCast<File>();

    m_cache.insertPath(path, file->id());

    qCDebug(GDRIVE) << "Resolved" << path << "to" << file->id() << "(from network)";
    return file->id();
}

QString KIOGDrive::resolveSharedDriveId(const QString &idOrName, const QString &accountId)
{
    qCDebug(GDRIVE) << "Resolving shared drive id for" << idOrName;

    const auto idOrNamePath = GDriveUrl::buildSharedDrivePath(accountId, idOrName);
    QString fileId = m_cache.idForPath(idOrNamePath);
    if (!fileId.isEmpty()) {
        qCDebug(GDRIVE) << "Resolved shared drive id" << idOrName << "to" << fileId << "(from cache)";
        return fileId;
    }

    // We start by trying to fetch a shared drive with the filename as id
    DrivesFetchJob searchByIdJob(idOrName, getAccount(accountId));
    searchByIdJob.setFields({
        Drives::Fields::Kind,
        Drives::Fields::Id,
        Drives::Fields::Name
    });
    QEventLoop eventLoop;
    QObject::connect(&searchByIdJob, &KGAPI2::Job::finished,
                     &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    if (searchByIdJob.error() == KGAPI2::OK || searchByIdJob.error() == KGAPI2::NoError) {
        // A Shared Drive with that id exists so we return it
        const auto objects = searchByIdJob.items();
        const DrivesPtr sharedDrive = objects.at(0).dynamicCast<Drives>();
        fileId = sharedDrive->id();
        qCDebug(GDRIVE) << "Resolved shared drive id" << idOrName << "to" << fileId;

        const auto idPath = idOrNamePath;
        const auto namePath = GDriveUrl::buildSharedDrivePath(accountId, sharedDrive->name());
        m_cache.insertPath(idPath, fileId);
        m_cache.insertPath(namePath, fileId);

        return fileId;
    }

    // The gdriveUrl's filename is not a shared drive id, we must
    // search for a shared drive with the filename name.
    // Unfortunately searching by name is only allowed for admin
    // accounts (i.e. useDomainAdminAccess=true) so we retrieve all
    // shared drives and search by name here
    DrivesFetchJob sharedDrivesFetchJob(getAccount(accountId));
    sharedDrivesFetchJob.setFields({
        Drives::Fields::Kind,
        Drives::Fields::Id,
        Drives::Fields::Name
    });
    QObject::connect(&sharedDrivesFetchJob, &KGAPI2::Job::finished,
                     &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    if (sharedDrivesFetchJob.error() == KGAPI2::OK || sharedDrivesFetchJob.error() == KGAPI2::NoError) {
        const auto objects = sharedDrivesFetchJob.items();
        for (const auto &object : objects) {
            const DrivesPtr sharedDrive = object.dynamicCast<Drives>();

            // If we have one or more hits we will take the first as good because we
            // don't have any other measures for picking the correct drive
            if (sharedDrive->name() == idOrName) {
                fileId = sharedDrive->id();
                qCDebug(GDRIVE) << "Resolved shared drive id" << idOrName << "to" << fileId;

                const auto idPath = GDriveUrl::buildSharedDrivePath(accountId, fileId);
                const auto namePath = idOrNamePath;
                m_cache.insertPath(idPath, fileId);
                m_cache.insertPath(namePath, fileId);

                return fileId;
            }
        }
    }

    // We couldn't find any shared drive with that id or name
    qCDebug(GDRIVE) << "Failed resolving shared drive" << idOrName << "(couldn't find drive with that id or name)";
    return QString();
}

QString KIOGDrive::rootFolderId(const QString &accountId)
{
    auto it = m_rootIds.constFind(accountId);
    if (it == m_rootIds.cend()) {
        qCDebug(GDRIVE) << "Getting root ID for" << accountId;
        AboutFetchJob aboutFetch(getAccount(accountId));
        aboutFetch.setFields({About::Fields::Kind, About::Fields::RootFolderId});
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

    if (gdriveUrl.isRoot())  {
        listAccounts();
        return;
    }
    if (gdriveUrl.isNewAccountPath()) {
        createAccount();
        return;
    }

    // We are committed to listing an url that belongs to
    // an account (i.e. not root or new account path), lets
    // make sure we know about the account
    const QString accountId = gdriveUrl.account();
    const auto account = getAccount(accountId);
    if (account->accountName().isEmpty()) {
        qCDebug(GDRIVE) << "Unknown account" << accountId << "for" << url;
        error(KIO::ERR_SLAVE_DEFINED, i18n("%1 isn't a known GDrive account", accountId));
        return;
    }

    QString folderId;
    if (gdriveUrl.isAccountRoot()) {
        auto entry = fetchSharedDrivesRootEntry(accountId);
        listEntry(entry);
        folderId = rootFolderId(accountId);
    } else if (gdriveUrl.isSharedDrivesRoot()) {
        listSharedDrivesRoot(url);
        return;
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
    const auto extraFields =
        QStringList({ KGAPI2::Drive::File::Fields::Labels,
                      KGAPI2::Drive::File::Fields::ExportLinks,
                      KGAPI2::Drive::File::Fields::LastViewedByMeDate,
                      KGAPI2::Drive::File::Fields::AlternateLink,
        });
    fileFetchJob.setFields(KGAPI2::Drive::FileFetchJob::FieldShorthands::BasicFields + extraFields);
    if (!runJob(fileFetchJob, url, accountId)) {
        return;
    }

    const ObjectsList objects = fileFetchJob.items();
    for (const ObjectPtr &object : objects) {
        const FilePtr file = object.dynamicCast<File>();

        const KIO::UDSEntry entry = fileToUDSEntry(file, url.adjusted(QUrl::StripTrailingSlash).path());
        listEntry(entry);

        const QString path = url.path().endsWith(QLatin1Char('/')) ? url.path() : url.path() + QLatin1Char('/');
        m_cache.insertPath(path + file->title(), file->id());
    }

    // We also need a non-null and writable UDSentry for "."
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
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

    if (gdriveUrl.isSharedDrive()) {
        qCDebug(GDRIVE) << "Directory is shared drive, creating that instead" << url;
        if (createSharedDrive(url)) {
            // runJob will have shown an error otherwise
            finished();
        }
        return;
    }

    QString parentId;
    if (gdriveUrl.isTopLevel()) {
        parentId = rootFolderId(accountId);
    } else {
        parentId = resolveFileIdFromPath(gdriveUrl.parentPath(), KIOGDrive::PathIsFolder);
    }

    if (parentId.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.path());
        return;
    }

    FilePtr file(new File());
    file->setTitle(gdriveUrl.filename());
    file->setMimeType(File::folderMimeType());

    ParentReferencePtr parent(new ParentReference(parentId));
    file->setParents(ParentReferencesList() << parent);

    FileCreateJob createJob(file, getAccount(accountId));
    if (runJob(createJob, url, accountId)) {
        finished();
    }
}

void KIOGDrive::stat(const QUrl &url)
{
    // TODO We should be using StatDetails to limit how we respond to a stat request
    // const QString statDetails = metaData(QStringLiteral("statDetails"));
    // KIO::StatDetails details = statDetails.isEmpty() ? KIO::StatDefaultDetails : static_cast<KIO::StatDetails>(statDetails.toInt());
    // qCDebug(GDRIVE) << "Going to stat()" << url << "for details" << details;

    const auto gdriveUrl = GDriveUrl(url);
    if (gdriveUrl.isRoot()) {
        // TODO Can we stat() root?
        finished();
        return;
    }
    if (gdriveUrl.isNewAccountPath()) {
        qCDebug(GDRIVE) << "stat()ing new-account path";
        const KIO::UDSEntry entry = newAccountUDSEntry();
        statEntry(entry);
        finished();
        return;
    }

    // We are committed to stat()ing an url that belongs to
    // an account (i.e. not root or new account path), lets
    // make sure we know about the account
    const QString accountId = gdriveUrl.account();
    const auto account = getAccount(accountId);
    if (account->accountName().isEmpty()) {
        qCDebug(GDRIVE) << "Unknown account" << accountId << "for" << url;
        error(KIO::ERR_SLAVE_DEFINED, i18n("%1 isn't a known GDrive account", accountId));
        return;
    }

    if (gdriveUrl.isAccountRoot()) {
        qCDebug(GDRIVE) << "stat()ing account root";
        const KIO::UDSEntry entry = accountToUDSEntry(accountId);
        statEntry(entry);
        finished();
        return;
    }
    if (gdriveUrl.isSharedDrivesRoot()) {
        qCDebug(GDRIVE) << "stat()ing Shared Drives root";
        const auto entry = fetchSharedDrivesRootEntry(accountId);
        statEntry(entry);
        finished();
        return;
    }
    if (gdriveUrl.isSharedDrive()) {
        qCDebug(GDRIVE) << "stat()ing Shared Drive" << url;
        statSharedDrive(url);
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
    if (!runJob(fileFetchJob, url, accountId)) {
        qCDebug(GDRIVE) << "Failed stat()ing file" << fileFetchJob.errorString();
        return;
    }

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
    fileFetchJob.setFields({File::Fields::Id, File::Fields::MimeType, File::Fields::ExportLinks, File::Fields::DownloadUrl});
    if (!runJob(fileFetchJob, url, accountId)) {
        return;
    }

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
    if (!runJob(contentJob, url, accountId)) {
        return;
    }

    QByteArray contentData = contentJob.data();

    totalSize(contentData.size());

    // data() has a maximum transfer size of 14 MiB so we need to send it in chunks.
    // See TransferJob::slotDataReq.
    int transferred = 0;
    // do-while loop to call data() even for empty files.
    do {
        const size_t nextChunk = qMin(contentData.size() - transferred, 14 * 1024 * 1024);
        data(QByteArray::fromRawData(contentData.constData() + transferred, nextChunk));
        transferred += nextChunk;
    } while (transferred < contentData.size());
    finished();
}

bool KIOGDrive::readPutData(QTemporaryFile &tempFile, FilePtr &fileMetaData)
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

    const QMimeType mime = QMimeDatabase().mimeTypeForFileNameAndData(fileMetaData->title(), &tempFile);
    fileMetaData->setMimeType(mime.name());

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
        qCDebug(GDRIVE) << "Running job" << (&job) << "with accessToken" << GDriveHelper::elideToken(job.account()->accessToken());
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

    FilePtr file = objects[0].dynamicCast<File>();

    QTemporaryFile tmpFile;
    if (!readPutData(tmpFile, file)) {
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

    if (!gdriveUrl.isTopLevel()) {
        // Not creating in root directory, fill parent references
        const QString parentId = resolveFileIdFromPath(gdriveUrl.parentPath());
        if (parentId.isEmpty()) {
            error(KIO::ERR_DOES_NOT_EXIST, url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).path());
            return false;
        }
        parentReferences << ParentReferencePtr(new ParentReference(parentId));
    }

    FilePtr file(new File);
    file->setTitle(gdriveUrl.filename());
    file->setParents(parentReferences);
    /*
    if (hasMetaData(QLatin1String("modified"))) {
        const QString modified = metaData(QLatin1String("modified"));
        qCDebug(GDRIVE) << modified;
        file->setModifiedDate(KDateTime::fromString(modified, KDateTime::ISODate));
    }
    */

    QTemporaryFile tmpFile;
    if (!readPutData(tmpFile, file)) {
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

    const auto gdriveUrl = GDriveUrl(url);

    if (gdriveUrl.isSharedDrive()) {
        qCDebug(GDRIVE) << "Can't create files in Shared Drives root" << url;
        error(KIO::ERR_CANNOT_WRITE, url.path());
        return;
    }

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
    sourceFileFetchJob.setFields({File::Fields::Id, File::Fields::ModifiedDate, File::Fields::LastViewedByMeDate, File::Fields::Description});
    if (!runJob(sourceFileFetchJob, src, sourceAccountId)) {
        return;
    }

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
    if (destGDriveUrl.isTopLevel()) {
        destDirId = rootFolderId(destAccountId);
    } else {
        destDirId = resolveFileIdFromPath(destGDriveUrl.parentPath(), KIOGDrive::PathIsFolder);
    }
    destParentReferences << ParentReferencePtr(new ParentReference(destDirId));

    FilePtr destFile(new File);
    destFile->setTitle(destGDriveUrl.filename());
    destFile->setModifiedDate(sourceFile->modifiedDate());
    destFile->setLastViewedByMeDate(sourceFile->lastViewedByMeDate());
    destFile->setDescription(sourceFile->description());
    destFile->setParents(destParentReferences);

    FileCopyJob copyJob(sourceFile, destFile, getAccount(sourceAccountId));
    if (runJob(copyJob, dest, sourceAccountId)) {
        finished();
    }
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

    const auto gdriveUrl = GDriveUrl(url);

    // Trying to delete the Team Drive root is pointless
    if (gdriveUrl.isSharedDrivesRoot()) {
        qCDebug(GDRIVE) << "Tried deleting Shared Drives root.";
        error(KIO::ERR_SLAVE_DEFINED, i18n("Can't delete Shared Drives root."));
        return;
    }

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

    if (gdriveUrl.isSharedDrive()) {
        qCDebug(GDRIVE) << "Deleting Shared Drive" << url;
        if (deleteSharedDrive(url)) {
            // error( will have been called in case of error
            finished();
        }
        return;
    }

    // GDrive allows us to delete entire directory even when it's not empty,
    // so we need to emulate the normal behavior ourselves by checking number of
    // child references
    if (!isfile) {
        ChildReferenceFetchJob referencesFetch(fileId, getAccount(accountId));
        if (!runJob(referencesFetch, url, accountId)) {
            return;
        }
        const bool isEmpty = !referencesFetch.items().count();

        if (!isEmpty && metaData(QStringLiteral("recurse")) != QLatin1String("true")) {
            error(KIO::ERR_CANNOT_RMDIR, url.path());
            return;
        }
    }

    FileTrashJob trashJob(fileId, getAccount(accountId));
    if (runJob(trashJob, url, accountId)) {
        m_cache.removePath(url.path());
        finished();
    }
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

    if (srcGDriveUrl.isSharedDrive()) {
        qCDebug(GDRIVE) << "Renaming Shared Drive" << srcGDriveUrl.filename() << "to" << destGDriveUrl.filename();
        DrivesPtr drives = DrivesPtr::create();
        drives->setId(sourceFileId);
        drives->setName(destGDriveUrl.filename());

        DrivesModifyJob modifyJob(drives, getAccount(sourceAccountId));
        if (!runJob(modifyJob, src, sourceAccountId)) {
            return;
        }

        finished();
        return;
    }

    // We need to fetch ALL, so that we can do update later
    FileFetchJob sourceFileFetchJob(sourceFileId, getAccount(sourceAccountId));
    if (!runJob(sourceFileFetchJob, src, sourceAccountId)) {
        return;
    }

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

    FilePtr destFile(sourceFile);
    destFile->setTitle(destGDriveUrl.filename());
    destFile->setParents(parentReferences);

    FileModifyJob modifyJob(destFile, getAccount(sourceAccountId));
    modifyJob.setUpdateModifiedDate(true);
    if (runJob(modifyJob, dest, sourceAccountId)) {
        finished();
    }
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
    fileFetchJob.setFields({File::Fields::Id, File::Fields::MimeType});
    if (!runJob(fileFetchJob, url, accountId)) {
        return;
    }

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
