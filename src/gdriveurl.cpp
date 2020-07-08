/*
 * Copyright (C) 2014 Daniel Vrátil <dvratil@redhat.com>
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
 * Copyright (c) 2019 David Barchiesi <david@barchie.si>
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

#include "gdriveurl.h"

const QString GDriveUrl::Scheme = QLatin1String("gdrive");
const QString GDriveUrl::SharedDrivesDir = QLatin1String("Shared Drives");
const QString GDriveUrl::TrashDir = QLatin1String("trash");
const QString GDriveUrl::NewAccountPath = QLatin1String("new-account");

GDriveUrl::GDriveUrl(const QUrl &url)
    : m_url(url)
{
    const auto path = url.adjusted(QUrl::StripTrailingSlash).path();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    m_components = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
#else
    m_components = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
#endif
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
