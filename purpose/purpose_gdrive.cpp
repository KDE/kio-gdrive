/*
    SPDX-FileCopyrightText: 2017 Lim Yuen Hoe <yuenhoe86@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "gdrivejob.h"
#include <purpose/pluginbase.h>

#include <KPluginFactory>
#include <QUrl>

class GDrivePlugin : public Purpose::PluginBase
{
    Q_OBJECT
public:
    GDrivePlugin(QObject *parent, const QVariantList &args)
        : Purpose::PluginBase(parent)
    {
        Q_UNUSED(args);
    }

    Purpose::Job *createJob() const override
    {
        return new GDriveJob(nullptr);
    }
};

K_PLUGIN_CLASS_WITH_JSON(GDrivePlugin, "purpose_gdrive.json")

#include "purpose_gdrive.moc"
