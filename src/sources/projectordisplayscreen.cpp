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

#include "../headers/projectordisplayscreen.hpp"
#include "ui_projectordisplayscreen.h"
#include <QDebug>

namespace {
// Qt 6 QML MediaPlayer.loops is a play COUNT (QMediaPlayer::Infinite == -1,
// QMediaPlayer::Once == 1). The old code passed QMediaPlaylist::Loop (== 3,
// "play three times") and QMediaPlaylist::CurrentItemOnce (== 0, which Qt 6
// ignores, so the previous loop count stuck).
constexpr int kLoopForever = -1;
constexpr int kPlayOnce = 1;
}


ProjectorDisplayScreen::ProjectorDisplayScreen(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProjectorDisplayScreen)
{
    ui->setupUi(this);
    dispView = new QQuickView;
    dispView->setResizeMode(QQuickView::SizeRootObjectToView);
    imProvider = new SpImageProvider;
    dispView->engine()->addImageProvider(QLatin1String("improvider"),imProvider);
    QWidget *w = QWidget::createWindowContainer(dispView,this);
    dispView->setSource(QUrl("qrc:/qml/qml/DisplayArea.qml"));
    ui->verticalLayout->addWidget(w);

    if (dispView->status() == QQuickView::Error || !dispView->rootObject())
    {
        // Typically a missing QtQuick/QtMultimedia QML plugin in a deployment.
        const auto errs = dispView->errors();
        for (const QQmlError &e : errs)
            qCritical() << "DisplayArea.qml:" << e.toString();
    }

    // Create Display object for retriaving signals back from the display screen
    QObject *dispObj = dispView->rootObject();
    connect(dispObj,SIGNAL(exitClicked()),this,SLOT(exitSlideClicked()));
    connect(dispObj,SIGNAL(nextClicked()),this,SLOT(nextSlideClicked()));
    connect(dispObj,SIGNAL(prevClicked()),this,SLOT(prevSlideClicked()));

    // Connect Media Player objects (QML signals emit int; we re-emit typed Qt6 enums)
    connect(dispObj, SIGNAL(positionChanged(int)), this, SLOT(onQmlPositionChanged(int)));
    connect(dispObj, SIGNAL(durationChanged(int)), this, SLOT(onQmlDurationChanged(int)));
    connect(dispObj, SIGNAL(playbackStateChanged(int)), this, SLOT(onQmlPlaybackStateChanged(int)));
    connect(dispObj, SIGNAL(playbackStopped()), this, SLOT(playbackStopped()));

    backImSwitch1 = backImSwitch2 = textImSwitch1 = textImSwitch2 = false;
    back1to2 = text1to2 = isNewBack = true;
    m_color.setRgb(0,0,0,0);// = QColor(QColor::black());
}

QObject *ProjectorDisplayScreen::qmlChild(const char *objectName) const
{
    QObject *root = dispView->rootObject();
    return root ? root->findChild<QObject*>(QLatin1String(objectName)) : nullptr;
}

ProjectorDisplayScreen::~ProjectorDisplayScreen()
{
    delete dispView;
    delete ui;
}

void ProjectorDisplayScreen::resetImGenSize()
{
    imGen.setScreenSize(this->size());
}

void ProjectorDisplayScreen::setBackPixmap(QPixmap p, int fillMode)
{
    // fill mode -->>  0 = Strech, 1 = keep aspect, 2 = keep aspect by expanding

    if(back.cacheKey() == p.cacheKey())
    {
        isNewBack = false;
        return;
    }
    back = p;
    isNewBack = true;

    switch(fillMode)
    {
    case 0:
        p = p.scaled(imGen.getScreenSize(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        break;
    case 1:
        p = p.scaled(imGen.getScreenSize(),Qt::KeepAspectRatio,Qt::SmoothTransformation);
        break;
    case 2:
        p = p.scaled(imGen.getScreenSize(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
        break;
    default:
        // Do No Scaling/resizing
        break;
    }

    imProvider->setPixMap(p);
    back1to2 = (!back1to2);

    QObject *item1 = qmlChild("backImage1");
    QObject *item2 = qmlChild("backImage2");
    if(item1 && item2)
    {
        if(back1to2)
        {
            backImSwitch2 = (!backImSwitch2);
            if(backImSwitch2)
                item2->setProperty("source","image://improvider/imB2a");
            else
                item2->setProperty("source","image://improvider/imB2b");

            if(p.height()<imGen.height())
                item2->setProperty("y",(imGen.height()-p.height())/2);
            else
                item2->setProperty("y",0);
            if(p.width()<imGen.width())
                item2->setProperty("x",(imGen.width()-p.width())/2);
            else
                item2->setProperty("x",0);
        }
        else
        {
            backImSwitch1 = (!backImSwitch1);
            if(backImSwitch1)
                item1->setProperty("source","image://improvider/imB1a");
            else
                item1->setProperty("source","image://improvider/imB1b");
            if(p.height()<imGen.height())
                item1->setProperty("y",(imGen.height()-p.height())/2);
            else
                item1->setProperty("y",0);
            if(p.width()<imGen.width())
                item1->setProperty("x",(imGen.width()-p.width())/2);
            else
                item1->setProperty("x",0);
        }
    }
}

void ProjectorDisplayScreen::setBackPixmap(QPixmap p, QColor c)
{
    if(backType == 0)
        setBackPixmap(imGen.generateColorImage(c),0);
    else if(backType == 1)
        setBackPixmap(p,0);
    else if(backType ==2)
        setBackPixmap(imGen.generateEmptyImage(),0);
}

void ProjectorDisplayScreen::setTextPixmap(QPixmap p)
{
    imProvider->setPixMap(p);
    text1to2 = (!text1to2);

    QObject *item1 = qmlChild("textImage1");
    QObject *item2 = qmlChild("textImage2");
    if(item1 && item2)
    {
        if(text1to2)
        {
            textImSwitch2 = (!textImSwitch2);
            if(textImSwitch2)
                item2->setProperty("source","image://improvider/imT2a");
            else
                item2->setProperty("source","image://improvider/imT2b");
        }
        else
        {
            textImSwitch1 = (!textImSwitch1);
            if(textImSwitch1)
                item1->setProperty("source","image://improvider/imT1a");
            else
                item1->setProperty("source","image://improvider/imT1b");
        }
    }
}

void ProjectorDisplayScreen::setBackVideo(QString path)
{
    QObject *item = qmlChild("player");
    QObject *item2 = qmlChild("vidOut");
    if(!item || !item2)
        return;

    setVideoSource(item,path);
    setVideoMuted(true);                      // background video is silent
    item->setProperty("loops",kLoopForever);
    item2->setProperty("fillMode",Qt::IgnoreAspectRatio);
}

void ProjectorDisplayScreen::setVideoSource(QObject* playerObject, QUrl path)
{
//#if  (defined(Q_OS_UNIX))
//    // Prefix "file://" to the file name only for Unix like systems.
//    path = "file://" + path;
//#endif
//    playerObject->setProperty("source",path);
    playerObject->setProperty("source",path.toString());
}

void ProjectorDisplayScreen::updateScreen()
{
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"stopTransitions");
    //    QString tranType = "seq";

    // if background is a video, play video, else stop it.
    if(backType == B_VIDEO)
    {
        QMetaObject::invokeMethod(root,"playVideo");
    }
    else
    {
        QMetaObject::invokeMethod(root,"stopVideo");
    }

    if(text1to2 && back1to2)
    {
        if(isNewBack)
            QMetaObject::invokeMethod(root,"transitionBack1to2",Q_ARG(QVariant,tranType));
        QMetaObject::invokeMethod(root,"transitionText1to2",Q_ARG(QVariant,tranType));
    }
    else if(text1to2 && (!back1to2))
    {
        if(isNewBack)
            QMetaObject::invokeMethod(root,"transitionBack2to1",Q_ARG(QVariant,tranType));
        QMetaObject::invokeMethod(root,"transitionText1to2",Q_ARG(QVariant,tranType));
    }
    else if((!text1to2) && back1to2)
    {
        if(isNewBack)
            QMetaObject::invokeMethod(root,"transitionBack1to2",Q_ARG(QVariant,tranType));
        QMetaObject::invokeMethod(root,"transitionText2to1",Q_ARG(QVariant,tranType));
    }
    else
    {
        if(isNewBack)
            QMetaObject::invokeMethod(root,"transitionBack2to1",Q_ARG(QVariant,tranType));
        QMetaObject::invokeMethod(root,"transitionText2to1",Q_ARG(QVariant,tranType));
    }
}

void ProjectorDisplayScreen::exitSlideClicked()
{
    emit exitSlide();
}

void ProjectorDisplayScreen::nextSlideClicked()
{
    emit nextSlide();
}

void ProjectorDisplayScreen::prevSlideClicked()
{
    emit prevSlide();
}

void ProjectorDisplayScreen::onQmlPositionChanged(int position)
{
    emit videoPositionChanged(static_cast<qint64>(position));
}

void ProjectorDisplayScreen::onQmlDurationChanged(int duration)
{
    emit videoDurationChanged(static_cast<qint64>(duration));
}

void ProjectorDisplayScreen::onQmlPlaybackStateChanged(int state)
{
    emit videoPlaybackStateChanged(static_cast<QMediaPlayer::PlaybackState>(state));
}

void ProjectorDisplayScreen::keyReleaseEvent(QKeyEvent *event)
{
    // Will get called when a key is released
    int key = event->key();
    if(key == Qt::Key_Left)
        emit prevSlide();
    else if(key == Qt::Key_Up)
        emit prevSlide();
    else if(key == Qt::Key_PageUp)
        emit prevSlide();
    else if(key == Qt::Key_Back)
        emit prevSlide();
    else if(key == Qt::Key_Right)
        emit nextSlide();
    else if(key == Qt::Key_Down)
        emit nextSlide();
    else if(key == Qt::Key_PageDown)
        emit nextSlide();
    else if(key == Qt::Key_Forward)
        emit nextSlide();
    else if(key == Qt::Key_Enter)
        emit nextSlide();
    else if(key == Qt::Key_Return)
        emit nextSlide();
    else if(key == Qt::Key_Escape)
        emit exitSlide();
    else
        QWidget::keyReleaseEvent(event);
}

void ProjectorDisplayScreen::renderNotText()
{
    setTextPixmap(imGen.generateEmptyImage());
    updateScreen();
}

void ProjectorDisplayScreen::renderPassiveText(QPixmap &back, bool useBack)
{
    setTextPixmap(imGen.generateEmptyImage());
    if(useBack)
    {
        backType = B_PICTURE;
        setBackPixmap(back,0);
    }
    else
    {
        backType = B_NONE;
        setBackPixmap(imGen.generateColorImage(m_color),0);
    }

    updateScreen();
}

void ProjectorDisplayScreen::renderBibleText(Verse bVerse, BibleSettings &bSets)
{
    // TODO: This is temporary until database is fixed
    if(bSets.useFading)
    {
        tranType = TR_FADE;
    }
    else
    {
        tranType = TR_NONE;
    }

    if(bSets.useBackground)
    {
        setBackPixmap(bSets.backgroundPix,0);
        backType = B_PICTURE;
    }
    else
    {
        setBackPixmap(imGen.generateColorImage(m_color),0);
        backType = B_NONE;
    }

    //tranType = bSets.transitionType;
    //backType = bSets.backgroundType;
    setTextPixmap(imGen.generateBibleImage(bVerse,bSets));
    //setBackPixmap(bSets.backgroundPix,bSets.backgroundColor);
    //if(backType ==2)
    //{
    //    setVideoSource(bSets.backgroundVideoPath);
    //}

    updateScreen();
}

void ProjectorDisplayScreen::renderSongText(Stanza stanza, SongSettings &sSets)
{
    // TODO: This is temporary until database is fixed
    if(sSets.useFading)
    {
        tranType = TR_FADE;
    }
    else
    {
        tranType = TR_NONE;
    }

    if(sSets.useBackground)
    {
        setBackPixmap(sSets.backgroundPix,0);
        backType = B_PICTURE;
    }
    else
    {
        setBackPixmap(imGen.generateColorImage(m_color),0);
        backType = B_NONE;
    }

    setTextPixmap(imGen.generateSongImage(stanza,sSets));
    //if(sSets.backgroundType == 1)
    //    setBackPixmap(sSets.backgroundPix,0);
    //else
    //    setBackPixmap(imGen.generateColorImage(m_color),0);
    //tranType = sSets.transitionType;
    updateScreen();
}

void ProjectorDisplayScreen::renderAnnounceText(AnnounceSlide announce, TextSettings &aSets)
{
    // TODO: This is temporary until database is fixed
    if(aSets.useFading)
    {
        tranType = TR_FADE;
    }
    else
    {
        tranType = TR_NONE;
    }

    if(aSets.useBackground)
    {
        setBackPixmap(aSets.backgroundPix,0);
        backType = B_PICTURE;
    }
    else
    {
        setBackPixmap(imGen.generateColorImage(m_color),0);
        backType = B_NONE;
    }

    setTextPixmap(imGen.generateAnnounceImage(announce,aSets));
    //if(aSets.transitionType == 1)
    //    setBackPixmap(aSets.backgroundPix,0);
    //else
    //    setBackPixmap(imGen.generateColorImage(m_color),0);
    //tranType = aSets.transitionType;
    updateScreen();
}

void ProjectorDisplayScreen::renderSlideShow(QPixmap slide, SlideShowSettings &ssSets)
{
    tranType = TR_FADE;

    bool expand;
    if(slide.width()<imGen.width() && slide.height()<imGen.height())
        expand = ssSets.expandSmall;
    else
        expand = true;

    setTextPixmap(imGen.generateEmptyImage());
    if(expand)
        setBackPixmap(slide,ssSets.fitType +1);
    else
        setBackPixmap(slide,3);
    updateScreen();

}

void ProjectorDisplayScreen::renderVideo(VideoInfo videoDetails)
{
    backType = B_VIDEO;
    setTextPixmap(imGen.generateEmptyImage());
    setBackPixmap(imGen.generateColorImage(m_color),0);
    QObject *item = qmlChild("player");
    QObject *item2 = qmlChild("vidOut");
    if(!item || !item2)
        return;

    setVideoSource(item,videoDetails.filePath);

    // Volume/mute are NOT touched here any more; callers set them afterwards.
    // (This used to reset volume to full, un-muting the secondary screens
    // right after SoftProjector::showVideo() muted them -> echo on multi-screen.)
    item->setProperty("loops",kPlayOnce);
    item2->setProperty("fillMode",Qt::KeepAspectRatio);

    updateScreen();
}

void ProjectorDisplayScreen::playVideo()
{
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"playVideo");
}

void ProjectorDisplayScreen::pauseVideo()
{
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"pauseVideo");
}

void ProjectorDisplayScreen::stopVideo()
{
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"stopVideo");
}

void ProjectorDisplayScreen::playbackStopped()
{
    emit videoStopped();
}

void ProjectorDisplayScreen::setVideoVolume(int level)
{
    const double vol = qBound(0, level, 100) / 100.0;
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"setVideoVolume",Q_ARG(QVariant,QVariant(vol)));
}

void ProjectorDisplayScreen::setVideoMuted(bool muted)
{
    QObject *root = dispView->rootObject();
    // Previously invoked setVideoVolume(!muted): unmuting jumped volume to 100%.
    QMetaObject::invokeMethod(root,"setVideoMuted",Q_ARG(QVariant,QVariant(muted)));
}

void ProjectorDisplayScreen::setVideoPosition(qint64 position)
{
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"setVideoPosition",Q_ARG(QVariant,QVariant(position)));
}

void ProjectorDisplayScreen::positionControls(DisplayControlsSettings &dSettings)
{
    //mySettings = dSettings;

    // set icon size
    int buttonSize(dSettings.buttonSize);
    if(buttonSize == 0)
        buttonSize = 16;
    else if(buttonSize == 1)
        buttonSize = 24;
    else if(buttonSize == 2)
        buttonSize = 32;
    else if(buttonSize == 3)
        buttonSize = 48;
    else if(buttonSize == 4)
        buttonSize = 64;
    else if(buttonSize == 5)
        buttonSize = 96;
    else
        buttonSize = 48;


    // calculate button position
    int y(this->height()), x(this->width()), margin(20);

    // calculate y position
    if(dSettings.alignmentV==0)//top
        y = margin;
    else if(dSettings.alignmentV==1)//middle
        y = (y-buttonSize)/2;
    else if(dSettings.alignmentV==2)//buttom
        y = y-buttonSize-margin;
    else
        y = y-buttonSize-margin;

    // calculate x position
    int xt((buttonSize*3)+20); //total width of the button group
    if(dSettings.alignmentH==0)
        x = margin;
    else if(dSettings.alignmentH==1)
        x = (x-xt)/2;
    else if (dSettings.alignmentH==2)
        x = x-xt-margin;
    else
        x = (x-xt)/2;

    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"positionControls",Q_ARG(QVariant,x),Q_ARG(QVariant,y),Q_ARG(QVariant,buttonSize),Q_ARG(QVariant,dSettings.opacity));

}

void ProjectorDisplayScreen::setControlsVisible(bool visible)
{
    QObject *root = dispView->rootObject();
    QMetaObject::invokeMethod(root,"setControlsVisible",Q_ARG(QVariant,visible));
}
