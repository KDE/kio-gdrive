/*
 * SPDX-FileCopyrightText: 2020 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef GDRIVEPROPERTIESPLUGIN_H
#define GDRIVEPROPERTIESPLUGIN_H

#include <KPropertiesDialog>
#include <KPropertiesDialogPlugin>

#include "ui_gdrivepropertiesplugin.h"

class GDrivePropertiesPlugin : public KPropertiesDialogPlugin
{
    Q_OBJECT
public:
    explicit GDrivePropertiesPlugin(QObject *parent, const QList<QVariant> &args);
    ~GDrivePropertiesPlugin() override = default;

private:
    QWidget m_widget;
    Ui::GDrivePropertiesWidget m_ui;
    KFileItem m_item;

    void showEntryDetails(const KIO::UDSEntry &entry);

private Q_SLOTS:
    void statJobFinished(KJob *job);
};

#endif // GDRIVEPROPERTIESPLUGIN_H
