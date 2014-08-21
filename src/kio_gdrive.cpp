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

#include "kio_gdrive.h"

#include <QtGui/QApplication>

#include <KDE/KComponentData>
#include <KDE/KDebug>
#include <KDE/KWallet/Wallet>
#include <KIO/Job>
#include <KIO/AccessManager>

#include <LibKGAPI2/Account>
#include <LibKGAPI2/AuthJob>
#include <LibKGAPI2/Drive/ChildReference>
#include <LibKGAPI2/Drive/ChildReferenceFetchJob>
#include <LibKGAPI2/Drive/ChildReferenceCreateJob>
#include <LibKGAPI2/Drive/File>
#include <LibKGAPI2/Drive/FileCreateJob>
#include <LibKGAPI2/Drive/FileFetchJob>
#include <LibKGAPI2/Drive/FileFetchContentJob>
#include <LibKGAPI2/Drive/FileSearchQuery>
#include <LibKGAPI2/Drive/ParentReference>
#include <LibKGAPI2/Drive/Permission>

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

using namespace KGAPI2;
using namespace Drive;

#define RUN_KGAPI_JOB(job) { \
    KIOGDrive::Action action = KIOGDrive::Fail; \
    do { \
        kDebug() << "Running job with accessToken" << job.account()->accessToken(); \
        QEventLoop eventLoop; \
        QObject::connect(&job, SIGNAL(finished(KGAPI2::Job*)), \
                          &eventLoop, SLOT(quit())); \
        eventLoop.exec(); \
        action = handleError(&job, url); \
        if (action == KIOGDrive::Success) { \
            break; \
        } else if (action == KIOGDrive::Fail) { \
            return; \
        } \
        job.setAccount(getAccount(accountId)); \
    } while (action == Restart); \
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
            const AccountPtr account = m_accountManager.refreshAccount(getAccount(oldAccount->accountName()));
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

KIO::UDSEntry KIOGDrive::fileToUDSEntry(const FilePtr &file) const
{
    KIO::UDSEntry entry;
    bool isFolder = false;

    entry.insert(KIO::UDSEntry::UDS_NAME, file->id());
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

QString KIOGDrive::lastPathComponent(const KUrl &url) const
{
    const QString path = url.path(KUrl::KUrl::RemoveTrailingSlash);
    const QStringList components = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (components.isEmpty()) {
        return QString();
    }
    if (components.size() == 1) {
        return QLatin1String("root");
    }
    return components.last();
}

QString KIOGDrive::accountFromPath(const KUrl& url) const
{
    const QString path = url.path(KUrl::RemoveTrailingSlash);
    const QStringList components = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (components.isEmpty()) {
        return QString();
    }
    return components[0];
}


void KIOGDrive::openConnection()
{
    kDebug() << "Ready to talk to GDrive";
}



void KIOGDrive::listDir(const KUrl &url)
{
    kDebug() << url;

    const QString folderId = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    // When listing root, list available accounts
    if (folderId.isEmpty()) {
        const QStringList accounts = m_accountManager.accounts();
        // If there are any accounts, list them and return
        if (!accounts.isEmpty()) {
            for (const QString &account : accounts) {
                const KIO::UDSEntry entry = AccountManager::accountToUDSEntry(account);
                listEntry(entry, false);
            }
            listEntry(KIO::UDSEntry(), true);
            finished();
            return;

        // otherwise ask user to authenticate and redirect to that account
        } else {
            const KGAPI2::AccountPtr account = m_accountManager.account(QString());
            redirection(KUrl(QString::fromLatin1("gdrive://%1").arg(account->accountName())));
            return;
        }
    }

    FileSearchQuery query;
    query.addQuery(FileSearchQuery::Parents, FileSearchQuery::In, folderId);
    FileFetchJob fileFetchJob(query, getAccount(accountId));
    fileFetchJob.setFields((FileFetchJob::BasicFields & ~FileFetchJob::Permissions)
                              | FileFetchJob::LastViewedByMeDate);
    RUN_KGAPI_JOB(fileFetchJob)

    ObjectsList objects = fileFetchJob.items();
    Q_FOREACH (const ObjectPtr &object, objects) {
        const FilePtr file = object.dynamicCast<File>();
        if (file->labels()->trashed()) {
            continue;
        }

        const KIO::UDSEntry entry = fileToUDSEntry(file);
        listEntry(entry, false);
    }

    listEntry(KIO::UDSEntry(), true);
    finished();
}


void KIOGDrive::mkdir(const KUrl &url, int permissions)
{
    kDebug() << url;

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
    Q_ASSERT(objects.count() == 1);

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
    fileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::MimeType | FileFetchJob::DownloadUrl);
    RUN_KGAPI_JOB(fileFetchJob)

    const ObjectsList objects = fileFetchJob.items();
    Q_ASSERT(objects.count() == 1);

    const FilePtr file = objects.first().dynamicCast<File>();

    mimeType(file->mimeType());

    FileFetchContentJob contentJob(file, getAccount(accountId));
    RUN_KGAPI_JOB(contentJob)

    data(contentJob.data());
    finished();
}

void KIOGDrive::put(const KUrl &url, int permissions, KIO::JobFlags flags)
{
    // TODO
    kDebug() << url << permissions << flags;
}



void KIOGDrive::copy(const KUrl &src, const KUrl &dest, int permissions, KIO::JobFlags flags)
{
    // TODO
    kDebug() << src << dest << permissions << flags;
}

void KIOGDrive::del(const KUrl &url, bool isfile)
{
    // TODO
    kDebug() << url << isfile;
}

void KIOGDrive::rename(const KUrl &src, const KUrl &dest, KIO::JobFlags flags)
{
    // TODO
    kDebug() << src << dest << flags;
}



void KIOGDrive::mimetype(const KUrl &url)
{
    kDebug() << url;

    const QString fileId = lastPathComponent(url);
    const QString accountId = accountFromPath(url);

    FileFetchJob fileFetchJob(fileId, getAccount(accountId));
    fileFetchJob.setFields(FileFetchJob::Id | FileFetchJob::MimeType);
    RUN_KGAPI_JOB(fileFetchJob);

    const ObjectsList objects = fileFetchJob.items();
    if (objects.count() != 1) {
        error(KIO::ERR_DOES_NOT_EXIST, url.fileName());
        return;
    }

    const FilePtr file = objects.first().dynamicCast<File>();
    mimeType(file->mimeType());
    finished();
}
