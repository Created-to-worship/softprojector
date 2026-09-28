##**************************************************************************
##
##    softProjector - an open source media projection software
##    Copyright (C) 2017  Vladislav Kobzar
##
##    This program is free software: you can redistribute it and/or modify
##    it under the terms of the GNU General Public License as published by
##    the Free Software Foundation version 3 of the License.
##
##**************************************************************************
##
## CMake is the primary build system (qmake's softProjector.pro is kept for
## legacy IDE setups). Qt Creator, Visual Studio and the GitHub Actions
## workflows under .github/workflows/ all build from this file.
##
## The source/header/UI/translation file lists below were transcribed
## directly from softProjector.pro so the two stay equivalent; if you add a
## new .cpp/.hpp/.ui file, add it in both places (or migrate fully to CMake
## and delete the .pro once you've verified the CMake build matches).
##**************************************************************************

cmake_minimum_required(VERSION 3.21)
project(SoftProjector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)   # Qt 6 requires C++17; nothing here needs C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# 6.4 is the hard minimum (Qt::StringLiterals / _L1 in managedatadialog.cpp);
# 6.8 LTS is what CI builds and what we recommend shipping.
# LinguistTools provides qt_add_translations() - without it configure fails
# with "Unknown CMake command qt_add_translations".
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui Widgets Network Sql Qml Quick PrintSupport Multimedia MultimediaWidgets
    LinguistTools
)

set(SP_SOURCES
    sources/main.cpp
    sources/softprojector.cpp
    sources/songwidget.cpp
    sources/biblewidget.cpp
    sources/editwidget.cpp
    sources/song.cpp
    sources/bible.cpp
    sources/settingsdialog.cpp
    sources/aboutdialog.cpp
    sources/addsongbookdialog.cpp
    sources/highlight.cpp
    sources/managedatadialog.cpp
    sources/managedata.cpp
    sources/announcewidget.cpp
    sources/helpdialog.cpp
    sources/songcounter.cpp
    sources/bibleinformationdialog.cpp
    sources/settings.cpp
    sources/generalsettingwidget.cpp
    sources/biblesettingwidget.cpp
    sources/songsettingwidget.cpp
    sources/announcementsettingwidget.cpp
    sources/printpreviewdialog.cpp
    sources/controlbutton.cpp
    sources/passivesettingwidget.cpp
    sources/theme.cpp
    sources/picturewidget.cpp
    sources/slideshow.cpp
    sources/mediawidget.cpp
    sources/videoplayerwidget.cpp
    sources/videoinfo.cpp
    sources/spfunctions.cpp
    sources/slideshoweditor.cpp
    sources/editannouncementdialog.cpp
    sources/announcement.cpp
    sources/schedule.cpp
    sources/picturesettingwidget.cpp
    sources/moduledownloaddialog.cpp
    sources/moduleprogressdialog.cpp
    sources/displaysetting.cpp
    sources/projectordisplayscreen.cpp
    sources/imagegenerator.cpp
    sources/spimageprovider.cpp
    sources/mediacontrol.cpp
)

set(SP_HEADERS
    headers/softprojector.hpp
    headers/songwidget.hpp
    headers/biblewidget.hpp
    headers/editwidget.hpp
    headers/song.hpp
    headers/bible.hpp
    headers/settingsdialog.hpp
    headers/aboutdialog.hpp
    headers/addsongbookdialog.hpp
    headers/highlight.hpp
    headers/managedatadialog.hpp
    headers/managedata.hpp
    headers/announcewidget.hpp
    headers/helpdialog.hpp
    headers/songcounter.hpp
    headers/bibleinformationdialog.hpp
    headers/settings.hpp
    headers/generalsettingwidget.hpp
    headers/biblesettingwidget.hpp
    headers/songsettingwidget.hpp
    headers/announcementsettingwidget.hpp
    headers/printpreviewdialog.hpp
    headers/controlbutton.hpp
    headers/passivesettingwidget.hpp
    headers/theme.hpp
    headers/picturewidget.hpp
    headers/slideshow.hpp
    headers/mediawidget.hpp
    headers/videoplayerwidget.hpp
    headers/videoinfo.hpp
    headers/spfunctions.hpp
    headers/slideshoweditor.hpp
    headers/editannouncementdialog.hpp
    headers/announcement.hpp
    headers/schedule.hpp
    headers/picturesettingwidget.hpp
    headers/moduledownloaddialog.hpp
    headers/moduleprogressdialog.hpp
    headers/displaysetting.hpp
    headers/projectordisplayscreen.hpp
    headers/imagegenerator.hpp
    headers/spimageprovider.hpp
    headers/mediacontrol.hpp
)

set(SP_UIS
    ui/softprojector.ui
    ui/songwidget.ui
    ui/biblewidget.ui
    ui/editwidget.ui
    ui/settingsdialog.ui
    ui/aboutdialog.ui
    ui/addsongbookdialog.ui
    ui/managedatadialog.ui
    ui/announcewidget.ui
    ui/helpdialog.ui
    ui/songcounter.ui
    ui/bibleinformationdialog.ui
    ui/generalsettingwidget.ui
    ui/biblesettingwidget.ui
    ui/songsettingwidget.ui
    ui/announcementsettingwidget.ui
    ui/printpreviewdialog.ui
    ui/passivesettingwidget.ui
    ui/picturewidget.ui
    ui/mediawidget.ui
    ui/slideshoweditor.ui
    ui/editannouncementdialog.ui
    ui/picturesettingwidget.ui
    ui/moduledownloaddialog.ui
    ui/moduleprogressdialog.ui
    ui/projectordisplayscreen.ui
    ui/mediacontrol.ui
)

qt_add_executable(SoftProjector WIN32
    ${SP_SOURCES}
    ${SP_HEADERS}
    ${SP_UIS}
    softprojector.qrc
)

target_include_directories(SoftProjector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/headers
)

target_link_libraries(SoftProjector PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql
    Qt6::Qml Qt6::Quick Qt6::PrintSupport Qt6::Multimedia Qt6::MultimediaWidgets
)

if(WIN32)
    target_sources(SoftProjector PRIVATE softprojector.rc)
endif()

# .qm files are compiled from the .ts sources at build time and embedded
# under :/translations/ (next to the flag icons listed in softprojector.qrc),
# so the language menu works without deploying a translations/ folder.
qt_add_translations(SoftProjector
    RESOURCE_PREFIX "/translations"
    TS_FILES
        translations/softpro_de.ts
        translations/softpro_ru.ts
        translations/softpro_cs.ts
        translations/softpro_ua.ts
        translations/softpro_hy.ts
)

if(MSVC)
    target_compile_options(SoftProjector PRIVATE /W3 /utf-8)
else()
    target_compile_options(SoftProjector PRIVATE -Wall -Wextra)
endif()

# On Windows, windeployqt (see CI) must ship the Qt Multimedia FFmpeg backend
# plugins; otherwise video presentation fails on minimal systems without codecs.
# The GitHub Actions workflow runs: windeployqt --qmldir src/qml ...
