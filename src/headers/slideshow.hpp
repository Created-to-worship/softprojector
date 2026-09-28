/***************************************************************************
//
//    softProjector - an open source media projection software
//    Copyright (C) 2017  Vladislav Kobzar
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation version 3 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
***************************************************************************/

#ifndef SLIDESHOW_HPP
#define SLIDESHOW_HPP

#include <QtSql>
#include <QString>
#include <QPixmap>
#include <QDebug>
#include <QProgressDialog>

#include "spfunctions.hpp"

class SlideShowItem
{
public:
    SlideShowItem();
    int slideId;
    int order;
    QString name;
    QString path;
    QPixmap image;
    QPixmap imageSmall;
    QPixmap imagePreview;

    // Full-size slides are kept encoded (as stored in the database) and decoded
    // on demand. Decoding every slide up front held ~3.7 MB per 1280px slide,
    // i.e. hundreds of MB for a large slide show.
    QByteArray imageData;
    void decodeImage();                 // image <- imageData (no-op if already decoded)
    void releaseImage();                // drop the decoded copy if it can be re-decoded
    QByteArray encodedImage() const;    // bytes to store: original encoding when available
};

class SlideShowInfo
{
public:
    SlideShowInfo();
    int slideSwId;
    QString name;
    QString info;
};

class SlideShow
{
public:
    SlideShow();
    int slideShowId;
    QString name;
    QString info;
    QList<SlideShowItem> slides;

public slots:
    void loadSlideShow(int id);
    void saveSideShow(QString savelbl, QWidget *ptW, QList<int> delList);
};

#endif // SLIDESHOW_HPP
