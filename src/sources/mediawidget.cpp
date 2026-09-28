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

#include <QMediaMetaData>

#include "../headers/mediawidget.hpp"
#include "ui_mediawidget.h"

namespace {
// The Media table stores plain local paths ("C:/Videos/a.mp4"). Wrapping that in
// QUrl(...) parses "C:" as a URL scheme, so media added in an earlier session
// could not be played on Windows. Accept both local paths and real URLs.
QUrl urlFromStoredPath(const QString &stored)
{
    if (stored.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)
        || stored.contains(QLatin1String("://")))
        return QUrl(stored);
    return QUrl::fromLocalFile(stored);
}

QString storedPathFromUrl(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}
}

MediaWidget::MediaWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MediaWidget)//,
{
    ui->setupUi(this);
    isReadyToPlay = false;
    player = new QMediaPlayer(this);
    // Qt6: QMediaPlayer no longer carries volume/mute itself - it plays through
    // a QAudioOutput sink, which is what now owns volume/mute state.
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);

    mediaControls = new MediaControl(this);
     ui->horizontalLayoutControls->addWidget(mediaControls);

    // Modernized (Qt6) connect syntax: pointer-to-member instead of the
    // old SIGNAL()/SLOT() string macros. This restores compile-time type
    // checking and fixes signals/slots that were silently renamed in Qt6
    // (stateChanged -> playbackStateChanged, error -> errorOccurred, and
    // volume/mute moving from QMediaPlayer to QAudioOutput).
    connect(player, &QMediaPlayer::metaDataChanged, this, &MediaWidget::updateInfo);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, &MediaWidget::statusChanged);
    connect(player, &QMediaPlayer::hasVideoChanged, this, &MediaWidget::hasVideoChanged);
    connect(player, &QMediaPlayer::errorOccurred, this, &MediaWidget::displayErrorMessage);

    connect(mediaControls, &MediaControl::muted, audioOutput, &QAudioOutput::setMuted);
    connect(mediaControls, &MediaControl::play, player, &QMediaPlayer::play);
    connect(mediaControls, &MediaControl::pause, player, &QMediaPlayer::pause);
    connect(mediaControls, &MediaControl::stop, player, &QMediaPlayer::stop);
    connect(mediaControls, &MediaControl::timeChanged, player, &QMediaPlayer::setPosition);
    connect(mediaControls, &MediaControl::volumeChanged, this, [this](int level) {
        audioOutput->setVolume(level / 100.0f); // MediaControl UI uses 0-100; QAudioOutput uses 0.0-1.0
    });

    connect(player, &QMediaPlayer::playbackStateChanged, mediaControls, &MediaControl::updatePlayerState);
    connect(player, &QMediaPlayer::durationChanged, mediaControls, &MediaControl::setMaximumTime);
    connect(player, &QMediaPlayer::positionChanged, mediaControls, &MediaControl::updateTime);
    connect(audioOutput, &QAudioOutput::volumeChanged, this, [this](float level) {
        // qRound, not a cast: 0.29f*100 == 28.99999 -> the slider crept down a step per change
        mediaControls->setVolume(qRound(level * 100.0f));
    });
  videoWidget = new VideoPlayerWidget(this);
    player->setVideoOutput(videoWidget);

    mediaControls->setVolume(100);


       ui->verticalLayoutMedia->addWidget(videoWidget);
        videoWidget->setVisible(false);




    /**********************************************/

    ui->pushButtonGoLive->setEnabled(false);

    playIcon = QIcon(":icons/icons/play.png");
    pauseIcon = QIcon(":icons/icons/pause.png");
    muteIcon = QIcon(":icons/icons/speakerMute.png");
    unmuteIcon = QIcon(":icons/icons/speaker.png");

    QPalette palette;
    palette.setBrush(QPalette::WindowText, Qt::white);

    ui->labelInfo->setStyleSheet("border-image:url(:icons/icons/playerScreen.png)");
    ui->labelInfo->setPalette(palette);
    ui->labelInfo->setText(tr("<center>No media</center>"));


//    connect(&mediaPlayer, SIGNAL(metaDataChanged()), this, SLOT(updateInfo()));
//    connect(videoWidget, SIGNAL(handleDrops(QDropEvent*)), this, SLOT(handleDrop(QDropEvent*)));

    setAcceptDrops(true);

    audioExt = "*.mp3 *.aac *.m4a *.flac *.opus *.ogg *.oga *.wma *.wav *.asf *.mka";   // was "*.acc" (typo)
    videoExt = "*.wmv *.avi *.mkv *.flv *.mp4 *.m4v *.webm *.mpg *.mpeg *.mov *.ogv *.ts";
    loadMediaLibrary();
}

MediaWidget::~MediaWidget()
{
    delete player;
    delete videoWidget;
    delete mediaControls;
//    delete timeSlider;
//    delete volumeSlider;
    delete ui;
}

void MediaWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MediaWidget::loadMediaLibrary()
{
    QSqlQuery sq;
    sq.exec("SELECT long_path, short_path FROM Media");
    while(sq.next())
    {
        mediaFilePaths.append(urlFromStoredPath(sq.value(0).toString()));
        mediaFileNames.append(sq.value(1).toString());
    }
    // (the list used to be cleared and refilled once per row - O(n^2))
    ui->listWidgetMediaFiles->clear();
    ui->listWidgetMediaFiles->addItems(mediaFileNames);
}

void MediaWidget::statusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug()<<"statusChanged"<<status;
    switch (status) {
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::LoadingMedia:
        ui->labelInfo->setText(QString("<center><strong><font color=#49fff9>%1</font>")
                               .arg(tr("Loading...")));
        break;
    case QMediaPlayer::StalledMedia:
        ui->labelInfo->setText(QString("<center><strong><font color=#49fff9>%1</font>")
                               .arg(tr("Media Stalled")));
        break;
    case QMediaPlayer::InvalidMedia:
        pendingGoLive = false;
        displayErrorMessage();
        break;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        isReadyToPlay = true;
        if(pendingGoLive)
        {
            pendingGoLive = false;
            goLiveNow();
        }
        break;
    default:
        break;
    }
}

void MediaWidget::displayErrorMessage()
{
    QString errMsg1 = tr("Possible Fail reasons:");
    QString errMsg2 = tr(" - Unsupported media format");
    QString errMsg3 = tr(" - Media file no longer exists or invalid path to file");

    if(!player->errorString().isEmpty())
    {
        errMsg1 = player->errorString();
        errMsg2 = errMsg3 = "";
    }
    ui->labelInfo->setText(QString("<center><strong><font color=#ff5555>%1:</font></strong><br>"
                                   "<font color=#49fff9>%2</font><br>%3<br>%4<br>%5</center>")
                           .arg(tr("ERROR Playing media file"))
                           .arg(currentMediaUrl.fileName())
                           .arg(errMsg1)
                           .arg(errMsg2)
                           .arg(errMsg3));
}

void MediaWidget::handleDrop(QDropEvent *e)
{
    QList<QUrl> urls = e->mimeData()->urls();
    QStringList fileList;
    QStringList fileNameList;

    // Make sure that only supported files get to media player
    // (compare real suffixes; the old regex left '.' unescaped)
    const QStringList exts = QString(audioExt + " " + videoExt).remove('*').remove('.')
                                 .split(' ', Qt::SkipEmptyParts);
    for(const QUrl &url : std::as_const(urls))
    {
        QString fp = url.toLocalFile();
        if(!fp.isEmpty() && exts.contains(QFileInfo(fp).suffix(), Qt::CaseInsensitive))
        {
            QFileInfo fn(fp);
            fileList.append(fp);
            fileNameList.append(fn.fileName());
        }
    }

    // Add files to library
    if(fileList.count()>0)
    {
        int mcount = ui->listWidgetMediaFiles->count(); // get total media count
        insertFiles(fileList);
        ui->listWidgetMediaFiles->setCurrentRow(mcount); // select first in list of just added and play it
    }
}

void MediaWidget::dropEvent(QDropEvent *e)
{
    if (e->mimeData()->hasUrls() && e->proposedAction() != Qt::LinkAction) {
        e->acceptProposedAction();
        handleDrop(e);
    } else {
        e->ignore();
    }
}

void MediaWidget::dragEnterEvent(QDragEnterEvent *e)
{
    dragMoveEvent(e);
}

void MediaWidget::dragMoveEvent(QDragMoveEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        if (e->proposedAction() == Qt::CopyAction || e->proposedAction() == Qt::MoveAction){
            e->acceptProposedAction();
        }
    }
}


void MediaWidget::playFile(QUrl filePath)
{
    isReadyToPlay = false;
    pendingGoLive = false;
    player->stop();
    currentMediaUrl = filePath;
    QUrl m(filePath);
    player->setSource(m);
}

void MediaWidget::updateInfo()
{
    int maxLength = 50;
    QString font = "<font color=#49fff9>";

    QString fName = player->source().fileName();
    QMediaMetaData md = player->metaData();
    QString tAlbum = md.stringValue(QMediaMetaData::AlbumTitle);
    QString tTitle = md.stringValue(QMediaMetaData::Title);
    QString tArtist = md.stringValue(QMediaMetaData::AlbumArtist);
    int tBitrate = md.value(QMediaMetaData::AudioBitRate).toInt();

    if (fName.length() > maxLength)
        fName = fName.left(maxLength) + "...";

    if (tAlbum.length() > maxLength)
        tAlbum = tAlbum.left(maxLength) + "...";

    if (tTitle.length() > maxLength)
        tTitle = tTitle.left(maxLength) + "...";

    if (tArtist.length() > maxLength)
        tArtist = tArtist.left(maxLength) + "...";

    QString file;
    if(!fName.isEmpty())
        file = "File: " + font + fName + "<br></font>";

    QString album;
    if(!tAlbum.isEmpty())
        album = "Album: " + font + tAlbum + "<br></font>";

    QString title;
    if(!tTitle.isEmpty())
    {
        title = "Title: " + font + tTitle + "<br></font>";
        file.clear();
    }

    QString artist;
    if (!tArtist.isEmpty())
        artist = "Artist: " + font + tArtist + "<br></font>";

    QString bitrate;
    if (tBitrate != 0)
        bitrate = "Bitrate: " + font + QString::number(tBitrate/1000) + "kbit</font>";

    ui->labelInfo->setText(file + album + title + artist + bitrate);

}

void MediaWidget::insertFiles(QStringList &files)
{
    // Insert filenames and path into appropriate lists and database
    // Bound parameters: a file such as "Bob's Song.mp3" used to break the INSERT
    QSqlQuery sq;
    sq.prepare("INSERT INTO Media (long_path, short_path) VALUES(?, ?)");
    for(const QString &file : std::as_const(files))
    {
        QFileInfo f(file);
        mediaFileNames.append(f.fileName());
        mediaFilePaths.append(QUrl::fromLocalFile(file));
        sq.bindValue(0, file);
        sq.bindValue(1, f.fileName());
        if(!sq.exec())
            qWarning() << "Media: could not add" << file << sq.lastError().text();
        ui->listWidgetMediaFiles->addItem(f.fileName());
    }
}

void MediaWidget::hasVideoChanged(bool bHasVideo)
{
    qDebug()<<"hasVideoChanged"<<bHasVideo;
    if(!bHasVideo && videoWidget->isFullScreen())
        videoWidget->setFullScreen(false);
    ui->labelInfo->setVisible(!bHasVideo);
    ui->pushButtonGoLive->setEnabled(bHasVideo);
    videoWidget->setVisible(bHasVideo);
//    isReadyToPlay = true;
}

void MediaWidget::prepareForProjection()
{
    player->pause();
    VideoInfo v;
//    v.fileName = mediaFileNames.at(ui->listWidgetMediaFiles->currentRow());
//    v.filePath = mediaFilePaths.at(ui->listWidgetMediaFiles->currentRow());
    v.fileName = currentMediaUrl.fileName();
    v.filePath = currentMediaUrl.toString();
    emit toProjector(v);
}

void MediaWidget::on_pushButtonGoLive_clicked()
{
    prepareForProjection();
}

void MediaWidget::addToLibrary()
{
    QStringList mfp = QFileDialog::getOpenFileNames(this,tr("Select Music/Video Files to Open"),".",
                                                    tr("Media Files (%1);;Audio Files (%2);;Video Files (%3)")
                                                    .arg(audioExt + " " + videoExt) // media files
                                                    .arg(audioExt) // audio files
                                                    .arg(videoExt)); // video files

    if(mfp.count()>0)
        insertFiles(mfp);
}

void MediaWidget::removeFromLibrary()
{
    int cm = ui->listWidgetMediaFiles->currentRow();
    if(cm>=0)
    {
        // Delete by full path (deleting by file name removed every same-named
        // file from other folders too) and with bound parameters.
        QSqlQuery sq;
        sq.prepare("DELETE FROM Media WHERE long_path = ? OR long_path = ?");
        sq.addBindValue(storedPathFromUrl(mediaFilePaths.at(cm)));
        sq.addBindValue(mediaFilePaths.at(cm).toString());
        sq.exec();
        mediaFilePaths.removeAt(cm);
        mediaFileNames.removeAt(cm);

        ui->listWidgetMediaFiles->setCurrentRow(-1);
        ui->listWidgetMediaFiles->clear();
        if(mediaFileNames.count()>0)
            ui->listWidgetMediaFiles->addItems(mediaFileNames);
        player->stop();

        hasVideoChanged(false);

        ui->labelInfo->setText(tr("<center>No media</center>"));
    }
}

void MediaWidget::on_listWidgetMediaFiles_itemSelectionChanged()
{
    int cRow = ui->listWidgetMediaFiles->currentRow();
    if(cRow>=0)
    {
        playFile(mediaFilePaths.at(cRow));
    }
}

void MediaWidget::on_listWidgetMediaFiles_doubleClicked(const QModelIndex &index)
{
    if(ui->pushButtonGoLive->isEnabled())
        prepareForProjection();
}

VideoInfo MediaWidget::getMedia()
{
    VideoInfo v;
    int cm = ui->listWidgetMediaFiles->currentRow();
    if(cm < 0 || cm >= mediaFileNames.count() || cm >= mediaFilePaths.count())
        return v;   // nothing selected (at(-1) used to crash)
    v.fileName = mediaFileNames.at(cm);
    v.filePath = mediaFilePaths.at(cm);
    return v;
}

void MediaWidget::setMediaFromSchedule(VideoInfo &v)
{
    if(currentMediaUrl == v.filePath)
    {
        // Same current Media File, do not update.
        return;
    }
    ui->listWidgetMediaFiles->clearSelection();
    playFile(v.filePath);
    player->pause();
}

void MediaWidget::goLiveFromSchedule()
{
    // This used to sleep() in a loop on the GUI thread until isReadyToPlay
    // became true. That flag is set by statusChanged(), which can only run once
    // the event loop is free again - so the whole application could hang forever.
    // Now we defer until the player reports LoadedMedia/BufferedMedia.
    if(isReadyToPlay)
        goLiveNow();
    else
        pendingGoLive = true;
}

void MediaWidget::goLiveNow()
{
    if(player->hasVideo())
        prepareForProjection();
    else
        player->play();   // audio only: play in the operator's player
}

bool MediaWidget::isValidMedia()
{
    if(ui->listWidgetMediaFiles->selectionModel()->selectedRows().count() > 0)
        return true;
    else
        return false;
}
