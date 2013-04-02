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

#include <LibKGAPI2/Account>
#include <LibKGAPI2/AuthJob>
#include <LibKGAPI2/Drive/ChildReference>
#include <LibKGAPI2/Drive/ChildReferenceFetchJob>
#include <LibKGAPI2/Drive/File>
#include <LibKGAPI2/Drive/FileFetchJob>
#include <LibKGAPI2/Drive/Permission>


using namespace KGAPI2;
using namespace Drive;

QString KIOGDrive::s_apiKey = QLatin1String( "554041944266.apps.googleusercontent.com" );
QString KIOGDrive::s_apiSecret = QLatin1String( "mdT1DjzohxN3npUUzkENT0gO" );

#define RUN_KGAPI_JOB(job) { \
    KIOGDrive::Action action = KIOGDrive::Fail; \
    do { \
        QEventLoop eventLoop; \
        QObject::connect( job, SIGNAL(finished(KGAPI2::Job*)), \
                          &eventLoop, SLOT(quit()) ); \
        eventLoop.exec(); \
        action = handleError( job, url ); \
        if ( action == KIOGDrive::Success ) { \
            break; \
        } else if ( action == KIOGDrive::Fail ) { \
            return; \
        } \
    } while ( action == Restart ); \
}

extern "C"
{
    int KDE_EXPORT kdemain( int argc, char **argv )
    {
        QApplication app( argc, argv );
        KComponentData( "kio_gdrive", "kdelibs4" );

        if (argc != 4) {
             fprintf(stderr, "Usage: kio_gdrive protocol domain-socket1 domain-socket2\n");
             exit(-1);
        }

        KIOGDrive slave( argv[1], argv[2], argv[3] );
        slave.dispatchLoop();
        return 0;
    }
}

KIOGDrive::KIOGDrive( const QByteArray &protocol, const QByteArray &pool_socket,
                      const QByteArray &app_socket ):
    SlaveBase( "gdrive", pool_socket, app_socket ),
    m_wallet( 0 )
{
    Q_UNUSED( protocol );

    kDebug() << "GDrive ready";

    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(
        KWallet::Wallet::NetworkWallet(), 0,
        KWallet::Wallet::Synchronous );
    if ( !wallet ) {
        return;
    }

    if ( !wallet->hasFolder( QLatin1String( "GDrive" ) ) ) {
        wallet->createFolder( QLatin1String( "GDrive" ) );
    }

    wallet->setFolder( QLatin1String("GDrive") );

    m_account = AccountPtr( new Account( QLatin1String( "dan.vratil@gmail.com" ) ) );
    if ( wallet->entryList().isEmpty() ) {
        m_account->addScope( QUrl( "https://www.googleapis.com/auth/drive" ) );
        m_account->addScope( QUrl( "https://www.googleapis.com/auth/drive.file") );
        m_account->addScope( QUrl( "https://www.googleapis.com/auth/drive.metadata.readonly" ) );
        m_account->addScope( QUrl( "https://www.googleapis.com/auth/drive.readonly" ) );

        AuthJob *authJob = new AuthJob( m_account, s_apiKey, s_apiSecret );

        QEventLoop eventLoop;
        QObject::connect( authJob, SIGNAL(finished(KGAPI2::Job*)),
                          &eventLoop, SLOT(quit()) );
        eventLoop.exec();

        m_account = authJob->account();
        authJob->deleteLater();

        QMap<QString, QString> entry;
        entry[ QLatin1String( "accessToken" ) ] = m_account->accessToken();
        entry[ QLatin1String( "refreshToken" ) ] = m_account->refreshToken();
        QStringList scopes;
        Q_FOREACH ( const QUrl &scope, m_account->scopes() ) {
            scopes << scope.toString();
        }
        entry[ QLatin1String( "scopes" ) ] = scopes.join( QLatin1String( "," ) );

        wallet->writeMap( m_account->accountName(), entry );

    } else {
        QMap<QString, QString> entry;
        wallet->readMap( m_account->accountName(), entry );

        m_account->setAccessToken( entry.value( QLatin1String( "accessToken" ) ) );
        m_account->setRefreshToken( entry.value( QLatin1String( "refreshToken" ) ) );
        const QStringList scopes = entry.value( QLatin1String( "scopes" ) ).split( QLatin1Char(',' ), QString::SkipEmptyParts );
        Q_FOREACH ( const QString &scope, scopes ) {
            m_account->addScope( scope );
        }
    }
}

KIOGDrive::~KIOGDrive()
{
    if ( m_wallet ) {
        m_wallet->closeWallet( KWallet::Wallet::NetworkWallet(), false );
    }

    closeConnection();
}

KIOGDrive::Action KIOGDrive::handleError( KGAPI2::Job *job, const KUrl &url )
{
    kDebug() << job->error() << job->errorString();

    switch ( job->error() ) {
        case KGAPI2::OK:
        case KGAPI2::NoError:
            return Success;
        case KGAPI2::AuthCancelled:
        case KGAPI2::AuthError:
            error( KIO::ERR_COULD_NOT_LOGIN, url.prettyUrl() );
            return Fail;
        case KGAPI2::Unauthorized: {
            AuthJob *authJob = new AuthJob( m_account, s_apiKey, s_apiSecret );
            QEventLoop eventLoop;
            QObject::connect( authJob, SIGNAL(finished(KGAPI2::Job*)),
                              &eventLoop, SLOT(quit()) );
            eventLoop.exec();

            if ( !handleError( authJob, url ) ) {
                error( KIO::ERR_COULD_NOT_LOGIN, url.prettyUrl() );
                return Fail;
            }
            return Restart;
        }
        case KGAPI2::Forbidden:
            error( KIO::ERR_ACCESS_DENIED, url.prettyUrl() );
            return Fail;
        case KGAPI2::NotFound:
            error( KIO::ERR_DOES_NOT_EXIST, url.prettyUrl() );
            return Fail;
        case KGAPI2::NoContent:
            error( KIO::ERR_NO_CONTENT, url.prettyUrl() );
            return Fail;
        case KGAPI2::QuotaExceeded:
            error( KIO::ERR_DISK_FULL, url.prettyUrl() );
            return Fail;
        default:
            error( KIO::ERR_SLAVE_DEFINED, job->errorString() );
            return Fail;
    }

    return Fail;
}

KIO::UDSEntry KIOGDrive::fileToUDSEntry(const FilePtr &file)
{
    KIO::UDSEntry entry;
    bool isFolder = false;

    entry.insert( KIO::UDSEntry::UDS_NAME, file->id() );
    entry.insert( KIO::UDSEntry::UDS_DISPLAY_NAME, file->title() );

    if ( file->isFolder() ) {
        entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR );
        entry.insert( KIO::UDSEntry::UDS_SIZE, 0 );
        isFolder = true;
    } else {
        entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG );
        entry.insert( KIO::UDSEntry::UDS_MIME_TYPE, file->mimeType() );
        entry.insert( KIO::UDSEntry::UDS_SIZE, file->fileSize() );
    }

    entry.insert( KIO::UDSEntry::UDS_CREATION_TIME, file->createdDate().toTime_t() );
    entry.insert( KIO::UDSEntry::UDS_MODIFICATION_TIME, file->modifiedDate().toTime_t() );
    entry.insert( KIO::UDSEntry::UDS_ACCESS_TIME, file->lastViewedByMeDate().toTime_t() );
    entry.insert( KIO::UDSEntry::UDS_USER, file->ownerNames().first() );

    if ( !isFolder ) {
        if ( file->editable() ) {
            entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        } else {
            entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH );
        }
    } else {
        entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
    }

    return entry;
}


void KIOGDrive::openConnection()
{
    kDebug() << "Ready to talk to GDrive";
}

void KIOGDrive::listDir( const KUrl &url )
{
    kDebug() << url;

    QString folderId;
    QString path = QUrl(url).toString( QUrl::StripTrailingSlash );
    if ( path.indexOf( QLatin1Char( '/' ) ) == -1 ) {
        folderId = QLatin1String( "root" );
    } else {
        folderId = path.mid( path.lastIndexOf( QLatin1Char('/') ) );
    }

    kDebug() << "Opening folder" << folderId;

    ChildReferenceFetchJob *fetchJob = new ChildReferenceFetchJob( folderId, m_account );
    RUN_KGAPI_JOB( fetchJob );

    ObjectsList objects = fetchJob->items();
    QStringList filesIds;
    Q_FOREACH ( const ObjectPtr &object, objects ) {
        const ChildReferencePtr ref = object.dynamicCast<ChildReference>();
        filesIds << ref->id();
    }

    kDebug() << "Found" << filesIds.count() << "entries";

    FileFetchJob *fileFetchJob =  new FileFetchJob( filesIds, m_account );
    RUN_KGAPI_JOB( fileFetchJob );

    objects = fileFetchJob->items();
    Q_FOREACH ( const ObjectPtr &object, objects ) {
        const FilePtr file = object.dynamicCast<File>();
        const KIO::UDSEntry entry = fileToUDSEntry( file );
        listEntry( entry, false );
    }

    listEntry( KIO::UDSEntry(), true );
    finished();
}

void KIOGDrive::stat(const KUrl &url)
{
    QString fileId;
    QString path = QUrl(url).toString( QUrl::StripTrailingSlash );
    if ( path.indexOf( QLatin1Char( '/' ) ) == -1 ) {
        fileId = QLatin1String( "root" );
    } else {
        fileId = path.mid( path.lastIndexOf( QLatin1Char('/') ) );
    }

    FileFetchJob *fileFetchJob = new FileFetchJob( fileId, m_account );
    RUN_KGAPI_JOB ( fileFetchJob );

    ObjectsList objects = fileFetchJob->items();
    Q_ASSERT( objects.count() == 1 );

    const FilePtr file = objects.first().dynamicCast<File>();
    const KIO::UDSEntry entry = fileToUDSEntry( file );

    statEntry( entry );
    finished();
}


