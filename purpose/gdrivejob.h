/*
    SPDX-FileCopyrightText: 2020 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef GDRIVEJOB_H
#define GDRIVEJOB_H

#include <QString>
#include <QUrl>
#include <purpose/pluginbase.h>

class GDriveJob : public Purpose::Job
{
    Q_OBJECT
public:
    GDriveJob(QObject *parent)
        : Purpose::Job(parent)
    {
    }
    void start() override;
};
#endif /* GDRIVEJOB_H */
