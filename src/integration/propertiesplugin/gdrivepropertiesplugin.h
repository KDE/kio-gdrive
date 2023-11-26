/*
 * SPDX-FileCopyrightText: 2020 David Barchiesi <david@barchie.si>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef GDRIVEPROPERTIESPLUGIN_H
#define GDRIVEPROPERTIESPLUGIN_H

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <KPropertiesDialog>
#else
#include <KPropertiesDialogPlugin>
#endif
#include <KPropertiesDialog>

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
