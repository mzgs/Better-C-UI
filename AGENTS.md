# AGENTS.md — Better-C-UI

Better-C-UI is an immediate-mode UI library in C99: about 190 components, a software and an OpenGL renderer, Win32, Cocoa, X11, SDL2 and GLFW backends. This guide applies throughout the repository. The human docs are the source of truth; keep this file in step with them.

## Read what the task needs

- Read the relevant parts of `docs/CONTRIBUTING.md` for the task: validation, code style and evidence. Its "If you are an AI agent" section applies when filing issues, pull requests or reviews.
- For library implementation work, read `docs/architecture.md` once; revisit the affected sections as needed. Use the table of contents in `src/cui_internal.h` to find shared helpers, then read their declarations and implementations as needed.
- For UI or interaction changes, read the relevant sections of `docs/design.md`.
- For documentation site changes, read `web/README.md` before building or editing generated files.
- Reuse context already read in this session. Search by symbol or section before opening whole files; inspect callers and ownership where behavior changes.

## Validation scope

Use the [validation table in CONTRIBUTING](docs/CONTRIBUTING.md#validation-scope). Choose checks from the affected behavior and dependencies, not merely the number of edited files.

- Documentation/instruction-only changes need link and diff checks, not C builds. Example-only changes build and exercise that example.
- Isolated component or add-on changes build affected targets and run relevant tests. Enable only the optional features involved. Do not run unrelated add-on suites or the full screenshot gallery.
- Shared frame, memory, layout, text, drawing, theme, internal structure, API contract or build-default changes require the full normal and text-shaping suites. Widen checks when impact is unclear or focused tests expose a shared regression.
- Run affected tests under sanitizers for memory, parsing, text or layout changes. Run the full sanitizer suite when the affected code is shared.
- For visual changes, inspect affected scenes in light and dark; update only images whose appearance changed. For behavior-preserving visual refactors, compare those scenes byte for byte. Use soak fingerprints for refactors affecting shared frame/layout/interaction behavior.
- Reuse compatible build directories; reconfigure when options or CMake inputs change. Once relevant checks pass, repeat only after changes that could affect them or unresolved failures. Report actual checks and remaining gaps concisely.

## Commands

Full normal suite (when required above):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Full text-shaping suite (when required above):

```sh
cmake -S . -B build-shaping -DCMAKE_BUILD_TYPE=Debug -DCUI_TEXT_SHAPING=ON
cmake --build build-shaping --parallel
ctest --test-dir build-shaping --output-on-failure
```

For focused checks, build the affected target and select existing tests with `ctest -R`; use `ctest -N` to inspect names. Sanitizer configuration is in `docs/CONTRIBUTING.md`. For affected visual scenes, render to a scratch directory, inspect light and dark images, then copy only intended changes into `docs/images/components/`:

```sh
mkdir -p /tmp/shots
./build/tests/cui_component_shots /tmp/shots [scene]       # component images
./build/tests/cui_screenshots /tmp/shots                   # gallery pages
```

`[scene]` is optional; omit it to render every scene. On Windows with a multi-configuration generator, use `--config Debug` for builds, `-C Debug` for CTest, and executables under `build/tests/Debug/`.

Behavior fingerprint for shared frame/layout/interaction refactors on Linux (deterministic; compare old and new library builds, each built with its own headers):

```sh
cc -O1 -DCUI_SOAK_NO_X11 -DSOAK_APP='"../examples/mail.c"' -I include -I src tests/soak.c build/libcui.a -lm -lpthread -ldl -o soak-mail
./soak-mail --minutes 2 --seed 5 --hash
```

## Rules

- **Video stays optional.** `CUI_VIDEO` builds `cui::video` in `addons/video/` against app-supplied libmpv. The core never includes or links libmpv. Keep callbacks thread-safe: only wake the app; UI/GL work and asynchronous controls run on the UI thread. Destroy players before their app/renderer. Exercise changed playback behavior with both renderers; include resize, pause/replay, hidden output and failed loading when lifecycle or rendering changes. Check the bundle when packaging or dependencies change.
- **Keep changes focused.** Preserve unrelated work. Use C99, 4 spaces, same-line braces, `cui_` public names and `cui__` internal names. Do not add default dependencies beyond the OS and vendored `third_party/`; `CUI_TREE_SITTER` builds a separate `cui::tree_sitter` add-on using an app-supplied runtime. Keep the core independent of add-on dependencies. Test Tree-sitter or shared highlighter integration changes with `-DCUI_TREE_SITTER=ON`. `CUI_TREE_SITTER_DEMO` separately downloads the pinned GPUI Kit grammars for the demo only.
- **Search before you write a helper.** Shared logic lives once in `cui_internal.h`: `CUI_C` for theme colors, `cui__emphasis` for the selected or active color, `cui__fs` for font sizes, `CUI__T_*` and `CUI__SPRING_*` for motion, `cui__draw_text_at` for text placed by a point. Do not copy a helper into another file.
- **Follow the component outline** in `docs/architecture.md`: options struct with a zero default, `_ex` function plus macro in `cui.h`, one state struct, paint data in frame memory, theme tokens instead of raw colors.
- **Complete component changes.** Update the public header comments, relevant tests, gallery, screenshot scenes and docs when affected. Support keyboard input, reduced motion, light and dark themes, narrow layouts and right-to-left interfaces as described in `docs/architecture.md`.
- **Split long functions by phase** (input, build, paint) with shared values in one struct. Do not add loose fields to `cui_ctx`; begin/end components use `ui->building`.
- **Names are unique across all files**, statics included, and no variable is named `near` or `far`: `tools/amalgamate.py` compiles the library as one file.
- **The core never calls into component files.** Optional parts install a hook (`ui->emoji_hook`).
- **Refactors must not change behavior.** Show it with the checks in Validation scope above, covering the changed paths. Keep refactors and behavior changes in separate commits.
- **Size matters.** Check `examples/hello` in a `MinSizeRel` build when you add code to the core.
- **Never commit secrets or private links:** no API keys, tokens, session URLs or private URLs in code, docs, commit messages or test data.
- **Report real evidence.** Follow `docs/CONTRIBUTING.md` for disclosure in issues, pull requests and reviews. Never invent test results; include actual commands and output, and say what was not checked. AI-assisted commits carry a `Co-Authored-By` trailer naming the model.
- **Every user-visible behavior or appearance change** gets a line in `CHANGELOG.md` and updates to affected docs; changed images belong in the same commit. Documentation/instruction-only edits need no changelog or regenerated images.
