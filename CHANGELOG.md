# Changelog

## Unreleased — performance, memory and modernization pass
- ~300× faster Bible verse display via database indexes (created automatically once).
- Much lower memory: song backgrounds and slide-show images are decoded on demand;
  long slide shows no longer hold every full-size slide in RAM.
- Fixed leaks in Song Counter, Print Preview (printer + dialog per click) and dialog handling.
- Fixed: blank slide in empty slide shows, slides dropped after an order gap,
  startup crash on songs with a deleted songbook.
- All C++ signal/slot connections are compile-time checked (pointer-to-member);
  static checker also verifies access rules.

## Unreleased — review & fix pass

Build
- Fixed three compile errors (ambiguous overloaded connects), a broken include
  in the vendored playlist code (now removed), a CMake configure error
  (missing LinguistTools) and a Windows CI job that could not find MSVC.
- Qt 6.4 minimum (6.8 LTS in CI), C++17, translations and flags embedded.
- New `tools/run_checks.py` static checks; CI: static checks + Linux + Windows builds.

Fixed
- Projector video had no picture/sound under Qt 6; volume, mute, seek and looping.
- Multi-screen video echo; app freeze when going live with media from a schedule.
- Schedule files corrupted by titles containing apostrophes; removed items not deleted.
- Media library unplayable after restart on Windows; apostrophes in media/songbook names.
- Window layout never restored; duplicated settings rows.
- Upgrade no longer loses the database (automatic copy from old locations).
- Hangs on truncated module downloads; lost file extensions on renamed downloads.
- Media tab mislabelled after switching language; "always on top" couldn't be turned off.

Removed
- Vendored QMediaPlaylist, dead display/settings forms, committed Makefile.
