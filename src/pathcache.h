/*
 * SPDX-FileCopyrightText: 2014 Daniel Vrátil <dvratil@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef PATHCACHE_H
#define PATHCACHE_H

#include <QHash>
#include <QStringList>

class PathCache
{
public:
    PathCache();
    ~PathCache();

    void insertPath(const QString &path, const QString &fileId);

    QString idForPath(const QString &path) const;
    QStringList descendants(const QString &path) const;
    void removePath(const QString &path);

    void dump();

private:
    QHash<QString /* path */, QString> m_pathIdMap;
};

#endif // PATHCACHE_H
