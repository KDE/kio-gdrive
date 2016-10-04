/*
 * Copyright (C) 2014 Daniel Vrátil <dvratil@redhat.com>
 * Copyright (c) 2016 Elvis Angelaccio <elvis.angelaccio@kde.org>
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

GDriveUrl::GDriveUrl(const QUrl &url)
    : m_url(url)
{
    const auto path = url.adjusted(QUrl::StripTrailingSlash).path();
    m_components = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
}

QString GDriveUrl::account() const
{
    if (isRoot()) {
        return QString();
    }

    return m_components.at(0);
}

bool GDriveUrl::isRoot() const
{
    return m_components.isEmpty();
}

bool GDriveUrl::isAccountRoot() const
{
    return m_components.length() == 1;
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
