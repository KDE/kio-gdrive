/*
 * Copyright (C) 2014  Daniel Vrátil <dvratil@redhat.com>
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

#ifndef PATHCACHE_H
#define PATHCACHE_H

#include <QHash>
#include <QStringList>

class PathCache
{
public:
    PathCache(int expireSeconds = 30);
    ~PathCache();

    void insertPath(const QString &id, const QString &name, const QString &parentId);

    QString nameForId(const QString &id);
    QString idForName(const QString &name);
    QStringList descendants(const QString &parentId);

private:
    class Record {
    public:
        Record();
        Record(const QString &value, const QString &parentId);
        Record(const Record &other);

        bool isNull() const {
            return timestamp == -1;
        }

        QString value;
        QString ancestor;
        qint64 timestamp;
    };

    bool isRecordExpired(const Record &record);
    QString valueForKey(QHash<QString, Record> &map, const QString &key);

    QHash<QString /* name */, Record > m_nameIdMap;
    QHash<QString /* id */,   Record > m_idNameMap;
    QMultiHash<QString /* parentId */, Record > m_parentChildrenMap;

    int m_expireSeconds;
};

#endif // PATHCACHE_H
