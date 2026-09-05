# Research: Qt i18n / qrc / QTranslator

## Sources

- Qt Resource System: https://doc.qt.io/qt-6/resources.html
- QTranslator: https://doc.qt.io/qt-6/qtranslator.html
- QCoreApplication translator API: https://doc.qt.io/qt-6/qcoreapplication.html#installTranslator

## Relevant External Guidance

- Qt resources compiled into a static library may need explicit resource initialization with `Q_INIT_RESOURCE`.
- `QTranslator` loads `.qm` files and `QCoreApplication::installTranslator` makes the newest installed translator participate in `tr()` lookup.
- Installing or removing a translator sends `LanguageChange` to application widgets, so widgets that keep text-bearing objects must handle retranslation.
- `QTranslator::isEmpty()` and direct `translate(context, sourceText)` probes are useful diagnostics after `load()`.

## Repository Evidence

- GUI worktree: `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`
- `src/app/main.cpp:43` calls `Q_INIT_RESOURCE(xq_resources)`.
- `resources/xq_resources.qrc:26-28` exposes `:/i18n/xq_zh_CN.qm`.
- `src/app/XQMainWindow.cpp:177-179` loads and installs the translator, but silently falls back to English on failure.
- `resources/i18n/xq_zh_CN.ts` has `XQMainWindow` translations for menus, actions, status labels, and MPR terms.
- `resources/i18n/xq_zh_CN.qm` exists and is non-empty.

## Design Implications

- Add a small diagnostic helper around translator loading:
  - check `QFile::exists(":/i18n/xq_zh_CN.qm")`;
  - record `load()` result;
  - check `!translator.isEmpty()`;
  - probe `translate("XQMainWindow", "&File")`.
- Add or update a Qt test that initializes `xq_resources`, loads the translator from qrc, and asserts a known source string translates to Chinese.
- Keep runtime language switching in one canonical translator path so English removal works.
- Do not treat missing Chinese as a font issue unless the `.qm` probe passes and translated Chinese still renders as boxes.

## Open Verification

- If the qrc probe passes but true UI remains English, inspect whether the user is running a stale `build_gui\xq_app.exe`, whether `retranslateUi()` misses any visible strings, or whether the translator is installed too late for specific widgets.
