/*
 * SPDX-FileCopyrightText: 2014 Daniel Vrátil <dvratil@redhat.com>
 * SPDX-FileCopyrightText: 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * SPDX-FileCopyrightText: 2019 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "gdriveurl.h"

const QString GDriveUrl::Scheme = QLatin1String("gdrive");
const QString GDriveUrl::SharedWithMeDir = QLatin1String("Shared With Me");
const QString GDriveUrl::SharedDrivesDir = QLatin1String("Shared Drives");
const QString GDriveUrl::TrashDir = QLatin1String("trash");
const QString GDriveUrl::NewAccountPath = QLatin1String("new-account");

GDriveUrl::GDriveUrl(const QUrl &url)
    : m_url(url)
{
    const auto path = url.adjusted(QUrl::StripTrailingSlash).path();
    m_components = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

QString GDriveUrl::account() const
{
    if (isRoot()) {
        return QString();
    }

    return m_components.at(0);
}

QString GDriveUrl::filename() const
{
    if (m_components.isEmpty()) {
        return QString();
    }

    return m_components.last();
}

bool GDriveUrl::isRoot() const
{
    return m_components.isEmpty();
}

bool GDriveUrl::isAccountRoot() const
{
    return m_components.length() == 1 && !isNewAccountPath();
}

bool GDriveUrl::isNewAccountPath() const
{
    return m_components.length() == 1 && m_components.at(0) == NewAccountPath;
}

bool GDriveUrl::isTopLevel() const
{
    return m_components.length() == 2;
}

bool GDriveUrl::isSharedWithMeRoot() const
{
    return m_components.length() == 2 && m_components.at(1) == SharedWithMeDir;
}

bool GDriveUrl::isSharedWithMeTopLevel() const
{
    return m_components.length() == 3 && m_components.at(1) == SharedWithMeDir;
}

bool GDriveUrl::isSharedWithMe() const
{
    return m_components.length() > 2 && m_components.at(1) == SharedWithMeDir;
}

bool GDriveUrl::isSharedDrivesRoot() const
{
    return m_components.length() == 2 && m_components.at(1) == SharedDrivesDir;
}

bool GDriveUrl::isSharedDrive() const
{
    return m_components.length() == 3 && m_components.at(1) == SharedDrivesDir;
}

bool GDriveUrl::isTrashDir() const
{
    return m_components.length() == 2 && m_components.at(1) == TrashDir;
}

bool GDriveUrl::isTrashed() const
{
    return m_components.length() > 2 && m_components.at(1) == TrashDir;
}

QUrl GDriveUrl::url() const
{
    return m_url;
}

QString GDriveUrl::parentPath() const
{
    if (isRoot()) {
        return QString();
    }

    auto path = m_components;
    path.removeLast();

    return QLatin1Char('/') + path.join(QLatin1Char('/'));
}

QStringList GDriveUrl::pathComponents() const
{
    return m_components;
}

QString GDriveUrl::buildSharedDrivePath(const QString &accountId, const QString &drive)
{
    return QStringLiteral("/%1/%2/%3").arg(accountId, SharedDrivesDir, drive);
}
