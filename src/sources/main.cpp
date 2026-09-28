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

#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QtSql>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QFile>
#include "../headers/softprojector.hpp"

// Definitions for database versions 'dbVer' numbers
// x - Official release. ex: 2 - for SoftProjector 2
// xxx - Official sub realeas. ex: 201 - for SoftProjector 2.01
// 990xxx - Development release. ex: 990206 - for SoftProjector 2 Development Build 6 (2db6)
int const dbVer = 2;

// Chooses the per-user data directory and, on first run, migrates data from the
// locations older builds used. Returns the path WITH a trailing '/'.
static QString resolveDataDir()
{
    // Local (not Roaming) AppData on Windows: spData.sqlite can be hundreds of MB
    // with Bibles, and SQLite's WAL mode (enabled below) does not work on the
    // network shares that roaming profiles are often redirected to.
    // On Linux/macOS AppLocalDataLocation == AppDataLocation.
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty())
        base = QCoreApplication::applicationDirPath();
    QDir dir(base);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        dir = QDir(QCoreApplication::applicationDirPath());   // very locked-down systems

    // One-time migration. Releases up to 2.2 kept spData.sqlite next to the
    // executable, and an earlier modernization pass used the *roaming* AppData
    // folder - without this, upgrading users would start with an empty
    // database. Files are copied, never moved, so nothing can be lost.
    const QString dbName = QStringLiteral("spData.sqlite");
    if (!QFile::exists(dir.filePath(dbName)))
    {
        const QStringList legacyDirs = {
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
            QCoreApplication::applicationDirPath() };
        for (const QString &from : legacyDirs)
        {
            if (from.isEmpty() || QDir(from) == dir || !QFile::exists(QDir(from).filePath(dbName)))
                continue;
            bool ok = true;
            const QStringList files = { dbName, dbName + QStringLiteral("-wal"),
                                        dbName + QStringLiteral("-shm"), QStringLiteral("DarkTheme.ini") };
            for (const QString &f : files)
            {
                const QString src = QDir(from).filePath(f), dst = dir.filePath(f);
                if (QFile::exists(src) && !QFile::exists(dst))
                {
                    if (QFile::copy(src, dst))   // copies can inherit a read-only flag
                        QFile(dst).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                  | QFileDevice::ReadUser | QFileDevice::WriteUser);
                    else if (f == dbName)
                        ok = false;
                }
            }
            qInfo() << "Data migration from" << from << "to" << dir.absolutePath() << (ok ? "succeeded" : "FAILED");
            if (ok)
                break;
        }
    }
    return dir.absolutePath() + QLatin1Char('/');
}

static bool openDatabase(QString database_file)
{
    database_file += "spData.sqlite";
    bool database_exists = ( QFile::exists(database_file) );

    // Named connection for clarity; enables safe removeDatabase later if needed
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");  // default connection; used app-wide
    db.setDatabaseName(database_file);
    if (!db.open())
    {
        QMessageBox mb;
        mb.setText("spData Error\n\n"
                   "Could not connect to the database spData.sqlite!\n\n"
                   "Following Errors:\n"
                   + db.lastError().databaseText() + "\n"
                   + db.lastError().driverText() +"\n"
                   + db.databaseName()
                   + "\n\nThis is a Fatal Error. Please make sure that all QtSql libraries are inlcuded."
                   "\nThe program will terminate");
        mb.setWindowTitle("Database Connection Error");
        mb.setIcon(QMessageBox::Critical);
        mb.exec();
        return false;
    }
    else
    {
        // Enable Write-Ahead Logging so concurrent readers (e.g. search) do not
        // block writers (edits) and vice versa. busy_timeout reduces SQLITE_BUSY
        // failures under load. Safe on existing DBs; no-op if already WAL.
        {
            QSqlQuery pragma(db);
            pragma.exec("PRAGMA journal_mode=WAL");
            pragma.exec("PRAGMA busy_timeout=5000");
            pragma.exec("PRAGMA synchronous=NORMAL");
        }

        // If no files existed, then database has been created; now fill schema
        if(!database_exists)
        {
            QSqlQuery sq;
            sq.exec(QString("PRAGMA user_version = %1").arg(dbVer));
            sq.exec("CREATE TABLE 'Announcements' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , "
                    "'title' TEXT, 'text' TEXT, 'usePrivate' BOOL, 'useAuto' BOOL, 'loop' BOOL, 'slideTime' INTEGER, "
                    "'useBackground' BOOL, 'backgoundPath' TEXT, 'font' TEXT, 'color' TEXT, 'alignment' TEXT)");
            sq.exec("CREATE TABLE 'BibleBooks' ('bible_id' INTEGER, 'id' INTEGER, 'book_name' "
                    "TEXT, 'chapter_count' INTEGER DEFAULT 0)");
            sq.exec("CREATE TABLE 'BibleVerse' ('verse_id' TEXT, 'bible_id' TEXT, 'book' TEXT, "
                    "'chapter' INTEGER, 'verse' INTEGER, 'verse_text' TEXT)");
            sq.exec("CREATE TABLE 'BibleVersions' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL, "
                    "'bible_name' TEXT, 'abbreviation' TEXT, 'information' TEXT, 'right_to_left' INTEGER DEFAULT 0)");
            sq.exec("CREATE TABLE 'Media' ('long_path' TEXT, 'short_path' TEXT)");
            sq.exec("CREATE TABLE 'Settings' ('type' TEXT, 'sets' TEXT)");
            sq.exec("CREATE TABLE 'SlideShows' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , 'name' TEXT, 'info' TEXT)");
            sq.exec("CREATE TABLE 'Slides' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , "
                    "'ss_id' INTEGER, 'p_order' INTEGER, 'name' TEXT, 'path' TEXT, "
                    "'pix' BLOB, 'pix_small' BLOB, 'pix_prev' BLOB)");
            sq.exec("CREATE TABLE 'Songbooks' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , 'name' TEXT, 'info' TEXT)");
            sq.exec("CREATE TABLE 'Songs' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , "
                    "'songbook_id' INTEGER, 'number' INTEGER, 'title' TEXT, 'category' INTEGER DEFAULT 0, "
                    "'tune' TEXT, 'words' TEXT, 'music' TEXT, 'song_text' TEXT, 'notes' TEXT, "
                    "'use_private' BOOL, 'alignment_v' INTEGER, 'alignment_h' INTEGER, 'color' INTEGER, 'font' TEXT, "
                    "'info_color' INTEGER, 'info_font' TEXT, 'ending_color' INTEGER, 'ending_font' TEXT, "
                    "'use_background' BOOL, 'background_name' TEXT, 'background' BLOB, 'count' INTEGER DEFAULT 0, 'date' TEXT)");
            sq.exec("CREATE TABLE 'ThemeAnnounce' ('theme_id' INTEGER, 'disp' INTEGER, 'use_shadow' BOOL, 'use_fading' BOOL, "
                    "'use_blur_shadow' BOOL, 'use_background' BOOL, 'background_name' TEXT, 'background' BLOB, 'text_font' TEXT, "
                    "'text_color' INTEGER, 'text_align_v' INTEGER, 'text_align_h' INTEGER, 'use_disp_1' BOOL)");
            sq.exec("CREATE TABLE 'ThemeBible' ('theme_id' INTEGER, 'disp' INTEGER, 'use_shadow' BOOL, 'use_fading' BOOL, "
                    "'use_blur_shadow' BOOL, 'use_background' BOOL, 'background_name' TEXT, 'background' BLOB, 'text_font' TEXT, "
                    "'text_color' INTEGER, 'text_align_v' INTEGER, 'text_align_h' INTEGER, 'caption_font' TEXT, "
                    "'caption_color' INTEGER, 'caption_align' INTEGER, 'caption_position' INTEGER, 'use_abbr' BOOL, "
                    "'screen_use' INTEGER, 'screen_position' INTEGER, 'use_disp_1' BOOL, "
                    "'add_background_color_to_text' BOOL, 'text_rec_background_color' INTEGER, 'text_gen_background_color' INTEGER)");
            sq.exec("CREATE TABLE 'ThemePassive' ('theme_id' INTEGER, 'disp' INTEGER, 'use_background' BOOL, "
                    "'background_name' TEXT, 'background' BLOB, 'use_disp_1' BOOL)");
            sq.exec("CREATE TABLE 'ThemeSong' ('theme_id' INTEGER, 'disp' INTEGER, 'use_shadow' BOOL, 'use_fading' BOOL, "
                    "'use_blur_shadow' BOOL, 'show_stanza_title' BOOL, 'show_key' BOOL, 'show_number' BOOL, "
                    "'info_color' INTEGER, 'info_font' TEXT, 'info_align' INTEGER, 'show_song_ending' BOOL, "
                    "'ending_color' INTEGER, 'ending_font' TEXT, 'ending_type' INTEGER, 'ending_position' INTEGER, "
                    "'use_background' BOOL, 'background_name' TEXT, 'background' BLOB, 'text_font' TEXT, "
                    "'text_color' INTEGER, 'text_align_v' INTEGER, 'text_align_h' INTEGER, "
                    "'screen_use' INTEGER, 'screen_position' INTEGER, 'use_disp_1' BOOL, "
                    "'add_background_color_to_text' BOOL, 'text_rec_background_color' INTEGER, 'text_gen_background_color' INTEGER)");
            //sq.exec("CREATE TABLE 'ThemeData' ('theme_id' INTEGER, 'type' TEXT, 'sets' TEXT)");
            sq.exec("CREATE TABLE 'Themes' ('id' INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , 'name' TEXT, 'comment' TEXT)");
        }
        return true;
    }
}

// Indexes for the hot queries. Without them every displayed verse was a full
// scan of BibleVerse (31,000 rows per installed Bible) - repeated for the
// primary/secondary/trinary Bible and for every display screen - and songbook,
// book and slide lookups scanned their tables too. IF NOT EXISTS keeps this
// idempotent; the one-time build on an existing database takes about a second.
static void ensureIndexes()
{
    QSqlQuery q;
    const char *statements[] = {
        "CREATE INDEX IF NOT EXISTS idx_BibleVerse_verse ON BibleVerse(verse_id, bible_id)",
        "CREATE INDEX IF NOT EXISTS idx_BibleVerse_bible ON BibleVerse(bible_id)",
        "CREATE INDEX IF NOT EXISTS idx_BibleBooks_bible ON BibleBooks(bible_id, id)",
        "CREATE INDEX IF NOT EXISTS idx_Songs_songbook   ON Songs(songbook_id, number)",
        "CREATE INDEX IF NOT EXISTS idx_Slides_slideshow ON Slides(ss_id, p_order)",
    };
    for (const char *sql : statements)
        if (!q.exec(QString::fromLatin1(sql)))
            qWarning() << "Index creation failed:" << q.lastError().text();
}

int main(int argc, char *argv[])
{
    // High-DPI: let Qt pass through fractional scale factors (125%/150%/etc.)
    // so multi-monitor projection geometry stays accurate on Windows 11 / modern macOS.
    // Must be set before QApplication is constructed.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);
    a.setApplicationName("SoftProjector");
    a.setOrganizationName("SoftProjector");
    a.setOrganizationDomain("softprojector.org");

    QPixmap pixmap(":icons/icons/splash.png");
    QSplashScreen splash(pixmap);
    splash.setMask(pixmap.mask());
    splash.show();
    a.processEvents();

//    QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));

    const QString database_dir = resolveDataDir();

#ifdef Q_OS_WIN
    // Use Dark Theme
    // NOTE (modernization): this previously read a registry value named
    // "SoftProjectorUseLightTheme", which Windows never actually writes -
    // that name does not exist, so this always read as "not set" and dark-mode
    // auto-detection never actually triggered from the real OS setting. The
    // real value Windows 10/11 write here is the DWORD "AppsUseLightTheme"
    // (0 = system is in Dark mode, 1 = Light mode). QSettings reads registry
    // DWORDs back as int, so this is compared as an int, not a QVariant.
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
    if(settings.value("AppsUseLightTheme", 1).toInt() == 0)
    {
        // Check if ini exists
        QString iniFilePath = database_dir + "DarkTheme.ini";
        bool darkTheme_Exists = ( QFile::exists(iniFilePath) );
        QString darkColor,disabledColor,custom,WindowText,Base,ToolTipBase,ToolTipText,Text,ButtonText,BrightText,Link,Highlight,HighlightedText;

        if(darkTheme_Exists)
        {
            // Load settings
            QSettings settings(iniFilePath, QSettings::IniFormat);
            darkColor = settings.value("Settings/darkColor","#2D2D2D").value<QString>();
            disabledColor = settings.value("Settings/disabledColor","#7F7F7F").value<QString>();
            custom = settings.value("Settings/custom").value<QString>();
            WindowText = settings.value("Settings/WindowText","#FFFFFF").value<QString>();
            Base = settings.value("Settings/Base","#121212").value<QString>();
            ToolTipBase = settings.value("Settings/ToolTipBase","#FFFFFF").value<QString>();
            ToolTipText = settings.value("Settings/ToolTipText","#FFFFFF").value<QString>();
            Text = settings.value("Settings/Text","#cccccc").value<QString>();
            ButtonText = settings.value("Settings/ButtonText","#FFFFFF").value<QString>();
            BrightText = settings.value("Settings/BrightText","#FF0000").value<QString>();
            Link = settings.value("Settings/Link","#2A82DA").value<QString>();
            Highlight = settings.value("Settings/Highlight","#15416D").value<QString>();
            HighlightedText = settings.value("Settings/HighlightedText","#FFFFFF").value<QString>();
        } else {
            // Create settings
            QSettings iniSettings(iniFilePath, QSettings::IniFormat);   // was heap-allocated and leaked
            iniSettings.setValue("Settings/darkColor","#2D2D2D");
            darkColor = "#2D2D2D";
            iniSettings.setValue("Settings/disabledColor","#7F7F7F");
            disabledColor = "#7F7F7F";
            iniSettings.setValue("Settings/custom","QToolTip { color: #ffffff; background-color: #15416d; border: 1px solid white; } QGroupBox { color: #e7d1ad; }");
            custom = "QToolTip { color: #ffffff; background-color: #15416d; border: 1px solid white; } QGroupBox { color: #e7d1ad; }";
            iniSettings.setValue("Settings/WindowText","#FFFFFF");
            WindowText = "#FFFFFF";
            iniSettings.setValue("Settings/Base","#121212");
            Base = "#121212";
            iniSettings.setValue("Settings/ToolTipBase","#FFFFFF");
            ToolTipBase = "#FFFFFF";
            iniSettings.setValue("Settings/ToolTipText","#FFFFFF");
            ToolTipText = "#FFFFFF";
            iniSettings.setValue("Settings/Text","#cccccc");
            Text = "#cccccc";
            iniSettings.setValue("Settings/ButtonText","#FFFFFF");
            ButtonText = "#FFFFFF";
            iniSettings.setValue("Settings/BrightText","#FF0000");
            BrightText = "#FF0000";
            iniSettings.setValue("Settings/Link","#2A82DA");
            Link = "#2A82DA";
            iniSettings.setValue("Settings/Highlight","#15416D");
            Highlight = "#15416D";
            iniSettings.setValue("Settings/HighlightedText","#FFFFFF");
            HighlightedText = "#FFFFFF";
            iniSettings.setValue("Example/Use_this_to_generate_Hex_codes","https://www.color-hex.com/color/ffffff");
            iniSettings.sync();
        }

        // Apply settings to palette
        QPalette darkPalette;
        qApp->setStyle(QStyleFactory::create("Fusion"));
        darkPalette.setColor(QPalette::Window, darkColor);
        darkPalette.setColor(QPalette::WindowText, WindowText);
        darkPalette.setColor(QPalette::Base, Base);
        darkPalette.setColor(QPalette::AlternateBase, darkColor);
        darkPalette.setColor(QPalette::ToolTipBase, ToolTipBase);
        darkPalette.setColor(QPalette::ToolTipText, ToolTipText);
        darkPalette.setColor(QPalette::Text, Text);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, darkColor);
        darkPalette.setColor(QPalette::ButtonText, ButtonText);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, BrightText);
        darkPalette.setColor(QPalette::Link, Link);
        darkPalette.setColor(QPalette::Highlight, Highlight);
        darkPalette.setColor(QPalette::HighlightedText, HighlightedText);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(darkPalette);
        qApp->setStyleSheet(custom);
    }
#endif

    // Try to connect to database
    if( !openDatabase(database_dir) )
        return 1;   // openDatabase() already showed a detailed error
    // Connected to the database OK:

    // Make sure that database is of correct version
    QSqlQuery sq;
    sq.exec("PRAGMA user_version");
    sq.first();
    int dbVersion = sq.value(0).toInt();
    if(dbVer != dbVersion)
    {
        QString errortxt = QString("SoftProjector requires database vesion # %1\n"
                                   "The database you are trying to open has vesion # %2\n"
                                   "Please use database with current version\n"
                                   "OR rename/remove all the database files\n"
                                   "and let softProjector to create needed database file.\n"
                                   "The program will terminate!"
                                   ).arg(dbVer).arg(dbVersion);
        QMessageBox mb;
        mb.setText(errortxt);
        mb.setWindowTitle("Incorrect Database Version");
        mb.setIcon(QMessageBox::Critical);
        mb.exec();
        return 1;
    }
    // Database is of correct version
    sq.finish();
    ensureIndexes();

    SoftProjector w;
    w.setAppDataDir(QDir(database_dir));
    w.show();
    splash.finish(&w);
    const int rc = a.exec();
    {
        // Let SQLite refresh its query-planner statistics (recommended at close)
        QSqlQuery optimize;
        optimize.exec(QStringLiteral("PRAGMA optimize"));
    }
    return rc;
}
