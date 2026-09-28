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

#include "../headers/slideshow.hpp"
#include <algorithm>

SlideShowItem::SlideShowItem()
{
    slideId = -1;
    order = -1;
}

SlideShowInfo::SlideShowInfo()
{
}

SlideShow::SlideShow()
{
    slideShowId = -1;
}

void SlideShow::loadSlideShow(int id)
{
    slides.clear();
    slideShowId = id;
    QSqlQuery sq;
    sq.exec(QString("SELECT name, info FROM SlideShows WHERE id = %1").arg(slideShowId));
    sq.first();
    name = sq.value(0).toString();
    info = sq.value(1).toString();

    QProgressDialog progress;
    progress.setMaximum(100);
    progress.setValue(1);
    sq.exec(QString("SELECT id, p_order, name, path, pix, pix_small, pix_prev FROM Slides WHERE ss_id = %1").arg(slideShowId));

    int max(0);
    while(sq.next())
        ++max;

    if(!sq.first())
        return;   // empty slide show (the do/while below used to add one blank slide)
    progress.setLabelText("Loading Slides...");
    progress.setMaximum(max+1);
    int ct(0);

    QList<SlideShowItem> ss;
    do
    {
        SlideShowItem si;
        si.slideId = sq.value(0).toInt();
        si.order = sq.value(1).toInt();
        si.name = sq.value(2).toString();
        si.path = sq.value(3).toString();
        si.imageData = sq.value(4).toByteArray();   // decoded on demand (decodeImage)
        si.imageSmall.loadFromData(sq.value(5).toByteArray());
        si.imagePreview.loadFromData(sq.value(6).toByteArray());

        ss.append(si);
        ++ct;
        progress.setValue(ct);
    }while(sq.next());

    // Sort to proper order number. (The old O(n^2) loop looked for orders
    // 0..n-1 only, so a slide after a gap in p_order was silently dropped.)
    std::stable_sort(ss.begin(), ss.end(), [](const SlideShowItem &a, const SlideShowItem &b) {
        return a.order < b.order;
    });
    slides.append(ss);

    ++ct;
    progress.setValue(ct);
}

void SlideShow::saveSideShow(QString savelbl, QWidget *ptW, QList<int> delList)
{
    QSqlQuery sq;
    QSqlDatabase::database().transaction();

    QProgressDialog prg(savelbl,"Cancel",0,slides.count()+1+delList.count(),ptW);

    int ct(0);

    // Create new slide show in not yet in database
    if(slideShowId == -1)
    {
        sq.prepare("INSERT INTO SlideShows (name, info) VALUES (?,?)");
        sq.addBindValue(name);
        sq.addBindValue(info);
        sq.exec();
        sq.clear();

        // get currently added slideShow id
        sq.exec("SELECT seq FROM sqlite_sequence WHERE name = 'SlideShows'");
        sq.first();
        slideShowId = sq.value(0).toInt();
        sq.clear();

        // Insert new slides
        sq.prepare("INSERT INTO Slides (ss_id, p_order, name, path, pix, pix_small, pix_prev) VALUES (?,?,?,?,?,?,?)");
        foreach(const SlideShowItem &si, slides)
        {
            sq.addBindValue(slideShowId);
            sq.addBindValue(ct);
            sq.addBindValue(si.name);
            sq.addBindValue(si.path);
            sq.addBindValue(si.encodedImage());
            sq.addBindValue(pixToByte(si.imageSmall));
            sq.addBindValue(pixToByte(si.imagePreview));
            sq.exec();
            ++ct;
            prg.setValue(ct);
        }
        QSqlDatabase::database().commit();
        return;
    }
    else
    {
        // Update Slide Show
        // Update slide show info
        sq.prepare("UPDATE SlideShows SET name = ?, info = ? WHERE id  = ?");
        sq.addBindValue(name);
        sq.addBindValue(info);
        sq.addBindValue(slideShowId);
        sq.exec();
        sq.clear();

        // Update slides (order only can be updated)
        int c(0);
        foreach(const SlideShowItem &si, slides)
        {
            if(si.slideId != -1)
            {
                sq.exec(QString("UPDATE Slides SET p_order = %1 WHERE id = %2").arg(c).arg(si.slideId));
                ++ct;
                prg.setValue(ct);
            }
            ++c;
        }
        sq.clear();

        // Insert new slides
        c = 0;
        sq.prepare("INSERT INTO Slides (ss_id, p_order, name, path, pix, pix_small, pix_prev) VALUES (?,?,?,?,?,?,?)");
        foreach(const SlideShowItem &si, slides)
        {
            if(si.slideId == -1)
            {
                sq.addBindValue(slideShowId);
                sq.addBindValue(c);
                sq.addBindValue(si.name);
                sq.addBindValue(si.path);
                sq.addBindValue(si.encodedImage());
                sq.addBindValue(pixToByte(si.imageSmall));
                sq.addBindValue(pixToByte(si.imagePreview));
                sq.exec();
                ++ct;
                prg.setValue(ct);
            }
            ++c;
        }
    }
    sq.clear();

    // Delete slides
    foreach(const int sid, delList)
    {
        if(sid>=0)
            sq.exec(QString("DELETE FROM Slides WHERE id = %1").arg(sid));
        ++ct;
        prg.setValue(ct);
    }

    prg.setLabelText("Saving Slide Show to Database");
    QSqlDatabase::database().commit();
    ++ct;
    prg.setValue(ct);
}

void SlideShowItem::decodeImage()
{
    if(image.isNull() && !imageData.isEmpty())
        image.loadFromData(imageData);
}

void SlideShowItem::releaseImage()
{
    if(!imageData.isEmpty())
        image = QPixmap();
}

QByteArray SlideShowItem::encodedImage() const
{
    return imageData.isEmpty() ? pixToByte(image) : imageData;
}
