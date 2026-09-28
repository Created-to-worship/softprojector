/***************************************************************************
//
//    softProjector - an open source media projection software
//    Copyright (C) 2017  Vladislav Kobzar
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation version 3 of the License.
//
***************************************************************************/

#include <QUrl>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include "../headers/helpdialog.hpp"
#include "ui_helpdialog.h"

HelpDialog::HelpDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HelpDialog)
{
    ui->setupUi(this);
    // Offline QtWebKit help was removed with the Qt6 migration.
    // Prefer the maintained online documentation.
}

HelpDialog::~HelpDialog()
{
    delete ui;
}

void HelpDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void HelpDialog::on_close_pushButton_clicked()
{
    close();
}

void HelpDialog::openOnlineHelp()
{
    const QUrl url(QStringLiteral("https://softprojector.org/help/index.html"));
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::information(this, tr("Help"),
            tr("Could not open the system browser.\n"
               "Please visit:\n%1").arg(url.toString()));
    }
}
