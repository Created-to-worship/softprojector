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

#include "../headers/mediacontrol.hpp"
#include "ui_mediacontrol.h"

MediaControl::MediaControl(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MediaControl)
{
    ui->setupUi(this);

    mPlayerState = QMediaPlayer::StoppedState;
    mDuration = 0;

    // Setup Icons
    mPlayIcon = QIcon(":icons/icons/play.png");
    mPauseIcon = QIcon(":icons/icons/pause.png");
    mMuteIcon = QIcon(":icons/icons/speakerMute.png");
    mUnmuteIcon = QIcon(":icons/icons/speaker.png");
}

MediaControl::~MediaControl()
{
    delete ui;
}

void MediaControl::setVolume(int level)
{
    ui->horizontalSliderVolume->setValue(level);
}

void MediaControl::updateTime(qint64 time)
{
    // Don't yank the handle away while the user is dragging it
    if(!ui->horizontalSliderTime->isSliderDown())
        ui->horizontalSliderTime->setValue(static_cast<int>(time/1000));

    QString timeString;
    if (time || mDuration)
    {
        const QString timeFormat = (mDuration >= 3600 * 1000) ? QStringLiteral("h:mm:ss")
                                                             : QStringLiteral("mm:ss");
        timeString = QTime(0, 0).addMSecs(static_cast<int>(time)).toString(timeFormat);
        if (mDuration)
            timeString += " / " + QTime(0, 0).addMSecs(static_cast<int>(mDuration)).toString(timeFormat);
    }
    ui->labelTime->setText(timeString);
}

void MediaControl::setMaximumTime(qint64 maxTime)
{
    mDuration = maxTime;
    ui->horizontalSliderTime->setMaximum(maxTime/1000);
}

void MediaControl::updatePlayerState(QMediaPlayer::PlaybackState state)
{
    mPlayerState = state;

    switch (mPlayerState)
    {
        case QMediaPlayer::StoppedState:
    case QMediaPlayer::PausedState:
        ui->pushButtonPlayPause->setIcon(mPlayIcon);
        break;
    case QMediaPlayer::PlayingState:
        ui->pushButtonPlayPause->setIcon(mPauseIcon);
        break;
    default:
        ui->pushButtonPlayPause->setIcon(mPlayIcon);
        break;
    }
}

void MediaControl::on_pushButtonStop_clicked()
{
    emit stop();
}

void MediaControl::on_pushButtonPlayPause_clicked()
{
    if(QMediaPlayer::PlayingState == mPlayerState)
    {
        emit pause();
    }
    else
    {
        emit play();
    }
}

void MediaControl::on_pushButtonMute_toggled(bool checked)
{
    emit muted(checked);

    ui->horizontalSliderVolume->setEnabled(!checked);

    if(checked)
    {
        ui->pushButtonMute->setIcon(mMuteIcon);
    }
    else
    {
        ui->pushButtonMute->setIcon(mUnmuteIcon);
    }
}

void MediaControl::on_horizontalSliderTime_sliderReleased()
{
    emit timeChanged(ui->horizontalSliderTime->value() * 1000);
}

void MediaControl::on_horizontalSliderVolume_valueChanged(int value)
{
    // valueChanged (not sliderMoved) so keyboard, mouse wheel and clicks on the
    // groove change the volume too. Receivers ignore unchanged values, so the
    // setVolume() -> valueChanged -> volumeChanged round trip terminates.
    emit volumeChanged(value);
}

