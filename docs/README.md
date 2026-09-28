# SoftProjector documentation

## Online help (preferred)

The application opens the maintained help site in the default browser:

https://softprojector.org/help/index.html

## Legacy bundled HTML

Historical offline help pages (English, German, Russian) and screenshots live under
`docs/help-legacy/`. They are **not** required at runtime. They are kept only as a
reference archive until migrated to a generated docs pipeline.

## Recommended future workflow

1. Author help in Markdown under `docs/src/` (one file per topic).
2. Build HTML with [MkDocs](https://www.mkdocs.org/) or Sphinx during packaging:

   ```bash
   pip install mkdocs
   mkdocs build -f docs/mkdocs.yml -d docs/site
   ```

3. Optionally ship `docs/site/` next to the binary, or host only on softprojector.org.

UI icons used by the running app remain under `src/icons/` and are embedded via
`softprojector.qrc` — those are runtime assets, not documentation screenshots.
