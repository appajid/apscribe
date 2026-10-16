# Cross-platform validation

Scribus fork changes target three primary desktop configurations:

- Windows x64 with Visual Studio 2022 and Qt 6
- Linux x64 with GCC and Qt 6
- macOS ARM64 with Clang and Qt 6

The `cross-platform-portability.yml` workflow compiles and runs the isolated,
platform-neutral tests on all three configurations. This gate is intentionally
small so it gives quick feedback for core logic without requiring every Scribus
packaging dependency.

Run the same gate locally from the repository root with:

```sh
cmake -S ci/portable -B build/portable -DCMAKE_BUILD_TYPE=Release
cmake --build build/portable --config Release --parallel 4
ctest --test-dir build/portable -C Release --output-on-failure
```

## Release validation levels

1. **Portable logic:** the GitHub Actions matrix must pass on Windows x64,
   Linux x64, and macOS ARM64.
2. **Full application build:** build Scribus with its complete dependency set on
   each platform. The checked-in Visual Studio projects remain part of the
   Windows build contract and must include every application source.
3. **Integration tests:** run the installed-application tests, including SLA
   persistence, Scripter, dynamic variables, running headers, cross-references,
   and anchored objects.
4. **GUI smoke testing:** manually verify startup, document creation, themes,
   typography/font fallback, image import, PDF output, printing, and HiDPI on
   representative machines.

A feature is not considered certified for a platform merely because its
portable tests pass. Release certification requires the full build and relevant
integration and GUI checks on that platform.

The current Phase 3 results and remaining certification gates are recorded in
[`PHASE_3_VALIDATION.md`](PHASE_3_VALIDATION.md).

## Anchored objects, inspector and Indic/RTL verification (17 October 2026)

This is a scoped follow-up to the older Phase 3 checklist, not a declaration
that all typography, accessibility or anchored editing operations are certified.
The verification branch is `codex/apscribe-workspace-verification-2026-10-17`;
public RC2 installers have not been replaced by these test-instrumented builds.

Reproduced and corrected:

- Locked anchored images/tables could still be dragged. Both drag preview and
  release now honor the object's lock, as well as the existing frame/layer locks.
- Changing paragraph direction and its associated alignment required multiple
  Undo actions. The inspector also used the last clicked alignment rather than
  the currently displayed one, which could overwrite centered/justified text.
  Direction/alignment are now one transaction, with no duplicate alignment signal.
- Inspector stacks did not safely handle an empty stack or explicit page minima;
  the Type/Paragraph stack included inactive pages in its size requirements.
  Active-page hints now propagate without forcibly resizing the user's dock.
  This addresses a concrete sizing cause related to upstream #167, not a claim
  to have reproduced its original Ubuntu/KDE/Qt 5 environment.

New coverage:

- `workspace_verification`: actual canvas mouse events select, drag and resize
  image/table anchors in inline, above-line and custom modes, in ordinary,
  rotated and mirrored text frames (18 scenarios). Checks include inspector
  targeting, image crop-offset isolation, object/position/size locks, undo/redo,
  table text retention and SLA save/reopen. Hidden Type/Paragraph page sizing is
  tested on the actual main-window inspector.
- `script_shaping`: ten Indic scripts, Arabic and Hebrew, including conjuncts,
  reordered marks and ZWJ/ZWNJ. Fourteen samples compare real glyph IDs, advances,
  mark offsets and source-character coverage against explicit HarfBuzz shaping
  with the same font. Mixed Arabic/Latin text and direction/font SLA persistence
  are checked. Twenty frame/table-cell scenarios exercise the real direction
  inspector across both directions and all five alignments, including one-step
  undo/redo and isolation of the following paragraph.
- `inspectorstacktests`: active-page resizing, explicit minimum sizes,
  expanded/collapsed size propagation and empty-stack safety; included in the
  Windows/Linux/macOS portable matrix.
- `workspace_verification` also exercises Light -> Dark -> Light through the
  toolbar commands and checks the application's and inspector's actual surface
  and text colours on native backends (and Linux's explicit fallback palettes).
  On macOS/Windows offscreen or minimal backends it checks commands/preferences
  only, explicitly logging that these cannot provide native palette evidence.

Mac/Qt 6.11.2: the rebuilt application and all plugins pass **64/64 CTest
tests** with `APSCRIBE_REQUIRE_SCRIPT_FONTS=1`, and **20/20 portable tests**.
All fourteen shaping samples had real font coverage; none were skipped. These
are installed-app/offscreen tests, not proof of every desktop gesture or script.
The normal macOS app also launched and opened the synthetic anchored fixture.
Both new native suites were subsequently repeated outside the sandbox using
the Cocoa backend: **21 workspace cases and 37 shaping/direction cases passed,
with no failures or skips**. The existing appearance self-test exercised
Light/Dark commands and restored its isolated `auto` preference; the workspace
cases remained green during that run. The subsequent actual-palette regression
test passed separately on native Cocoa (three QtTest cases including setup and
cleanup), followed by the complete expanded workspace suite (**22 passed, zero
failures/skips**). The complete Mac CTest suite was also repeated, **64/64
passed**. A desktop-control click had changed the button's checked state without
changing the preference; that observation was not reproduced by the actual
toolbar commands, so no production theme patch was made.
An initial desktop-input service error was subsequently resolved for the
anchored-image walkthrough: direct selection displayed handles, canvas dragging
and handle resizing worked, and two Undo actions restored the original geometry
without saving the document. The tester reported that tables could be dragged
and subsequently confirmed the unlocked image was working too. This is scoped
Mac gesture evidence, not certification of every anchored-object operation.

The Scripter persistence fixture deliberately saves the image with manual
positioning locked. It now also creates and reopens a separate
`apscribe_anchored_manual_drag.sla` with image/table manual positioning and
keep-within-bounds constraints disabled, checking that both settings and the
linked image survive reopening. Use this separate file for free-drag acceptance
testing; the original lock-persistence assertions remain intact.

Linux CI installs `fonts-noto-core` and `fonts-dejavu-core` and requires all
script samples to run. The mixed Arabic/Latin test checks full coverage in a
single face; DejaVu Sans provides the explicit Linux candidate without relying
on font substitution.
Windows uses an opt-in `APSCRIBE_VERIFICATION_TESTS=true` main-project import
(VS 2019/2022/2026) to compile the same native suites and link QtTest. Ordinary
release builds do not enable this import. The Windows verification runner
requires native `qwindows` startup, complete font coverage and zero skipped
cases; its reports and rendered samples are retained as Actions artifacts.

Initial CI runs for the verification commit:

- [Linux native verification](https://github.com/appajid/apscribe/actions/runs/38957653312)
- [Windows native verification](https://github.com/appajid/apscribe/actions/runs/38957653213)
- [Three-platform portable checks](https://github.com/appajid/apscribe/actions/runs/38957653244)

The three-platform portable checks passed. The initial Linux run compiled the
full application/plugins and passed Phase 5, anchored-object and inspector
checks. All fourteen script-shaping samples and twenty direction-inspector
cases passed; the mixed Arabic/Latin persistence case failed because no listed
font covered both scripts. The explicit DejaVu dependency/candidate above
corrects the test setup; a rerun is required before claiming Linux success.
The Windows full build was still in progress at the latest check. Do not infer
its success from the Mac results. Remaining acceptance includes native
light/dark and HiDPI walkthroughs, screen-reader/focus coverage, specialist Indic
and RTL visual review with multiple font families, linked-story/page-boundary
stress, grouped/shape anchors, copy/paste/unanchor, and PDF/print fidelity.
