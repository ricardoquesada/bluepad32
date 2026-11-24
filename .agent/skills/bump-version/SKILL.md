---
name: bump-version
description: >-
  Bumps Bluepad32 version files (uni_version.h, idf_component.yml), updates
  AUTHORS.md, and writes user-facing CHANGELOG.md release notes in Simplified
  Technical English (ASD-STE100). Use when the user asks to bump the version or
  prepare a release (for example, /bump-version v5.0.0-beta1 or /bump-version
  5.0.0). Don't use for git commits or tags unless explicitly requested.
---

# Bump Version Skill

## 1. Verify the New Version

Check that the user provided a valid semantic version (strip any leading `v`,
such as `v5.0.0-beta1` -> `5.0.0-beta1` or `v5.0.0` -> `5.0.0`). Split the
numeric core (`MAJOR`, `MINOR`, `PATCH`) from any optional prerelease suffix
(for example `-beta1`, `-rc0`). If the user omitted the version, ask for
clarification first.

## 2. Update Version Files

Update every tracked Bluepad32 version definition:

- `src/components/bluepad32/include/uni_version.h`:
  - Update `UNI_VERSION_STRING` to `"<version>"` (including any prerelease
    suffix).
  - Update numeric `UNI_VERSION_MAJOR`, `UNI_VERSION_MINOR`, and
    `UNI_VERSION_PATCH`.
  - Check whether the `BTSTACK_VERSION_AT_LEAST(...)` minimum version check
    needs an update if the `external/btstack` submodule bumped in this release.
- `src/components/bluepad32/idf_component.yml`:
  - Update `version: "<version>"`.
  - Check whether `dependencies.idf.version` needs an update if the minimum
    ESP-IDF requirement changed.
- `src/components/bluepad32/uni_init.c`:
  - Check the startup log banner copyright year (`2016-YYYY`) and update it if
    the year changed.

Note: `src/components/bluepad32/uni_version.c` reads `UNI_VERSION_STRING`, and
`docs/release_notes.md` includes `../CHANGELOG.md` automatically, so neither file
needs manual version edits.

## 3. Review Recent Changes

List all commits since the previous release tag (using `--no-merges` or full
commit bodies as needed):

```bash
git log $(git describe --tags --abbrev=0)..HEAD --format='=== %h %s ===%n%b'
```

Group commits by user impact rather than writing one bullet per commit. Omit
internal refactors with no user-visible effect and omit fixes for bugs
introduced in the same unreleased cycle.

## 4. Update CHANGELOG.md (User-Facing "What" + ASD-STE100)

In `CHANGELOG.md`:

- If a placeholder section for the target version already exists (such as
  `## [5.0.0-beta1] - 2026-??-??`), update its date to `YYYY-MM-DD` and
  refresh or expand its entries.
- Otherwise, insert `## [<version>] - YYYY-MM-DD` (with the current date)
  above the previous release header.
- Group entries under Bluepad32's headings: `### New`, `### Changed`, and
  `### Fixed`. Prefix each bullet with its subsystem (for example `ESP32:`,
  `Pico W:`, `BLE:`, `Switch 2:`, `Steam:`, `DualSense:`, `SInput:`,
  `Unijoysticle:`, `Examples:`, `Tests & CI:`), and place `[github_issue_NNN]`
  / `[github_pr_NNN]` reference definitions cleanly with the rest of the section.

Apply the `asd-ste100` skill (when available) and enforce both rules below:

1. **Focus on "What", Not "How"**: `CHANGELOG.md` serves firmware and library
   users of Bluepad32, not maintainers reading `git log`. State user-visible
   controllers, boards, protocols, SDK/BTstack requirements, public API changes,
   examples, and fixed symptoms. Omit internal `static` helper names, file
   reordering, private parser structs, and implementation mechanics.
2. **Use ASD-STE100 Simplified Technical English**:
   - Start entries with active imperative or direct verbs (`Support`, `Add`,
     `Update`, `Fix`, `Remove`, `Keep`).
   - Keep sentences short (`<= 20` words for actions, `<= 25` words for
     descriptions), with one main idea per sentence.
   - Use simple tenses only. Avoid `-ing` participles, passive voice, phrasal
     verbs (`fall back`, `spin up`), prose semicolons, and filler adjectives
     (`comprehensive`, `seamless`, `robust`).

### Example

- **Avoid ("how" + internal symbols)**:
  `Refactored uni_hid_parser_steam_triton.c static helpers into SSSSS->PPPP
  order and fixed sw2_advance_command_state GATT write pacing retries.`
- **Prefer ("what" + ASD-STE100)**:
  `Switch 2: Fix connection timeouts during BLE setup so controllers pair
  reliably.`

## 5. Update AUTHORS.md

List all non-maintainer code contributors since the previous release tag, and
check copyright/author headers in newly added files (in case ported code was
committed under the maintainer's Git author):

```bash
git log $(git describe --tags --abbrev=0)..HEAD --format='%h %an: %s'
git shortlog -sn $(git describe --tags --abbrev=0)..HEAD
git diff --diff-filter=A --name-only $(git describe --tags --abbrev=0)..HEAD -- '*.c' '*.h' '*.cpp' '*.hpp' '*.py' '*.md' \
  | while IFS= read -r f; do echo "=== $f ==="; head -n 5 "$f"; done
```

Always update `AUTHORS.md` under `## Code contributors` using the file's
existing 4-space indented bullet format (`- Name:` followed by `    - ...`):

- Skip the main author (`Ricardo Quesada`) and automated bots.
- For each existing contributor in `## Code contributors` who landed new
  user-visible commits in this release, append concise bullet(s) under their
  existing entry.
- Append any new code contributor (from Git history or file headers) at the
  bottom of `## Code contributors` with concise bullet(s) summarizing their
  contributions.

## 6. Verify and Notify User

1. Verify that `uni_version.h`, `idf_component.yml`, `CHANGELOG.md`, and
   `AUTHORS.md` all reflect the target `<version>` and its contributors with no
   stale version strings left behind.
2. Run formatting and host unit tests (which validate `UNI_VERSION_STRING` in
   `test_uni_property_and_bb`):

```bash
clang-format --dry-run --Werror \
  src/components/bluepad32/include/uni_version.h \
  src/components/bluepad32/uni_init.c
cmake -S tests -B tests/build && cmake --build tests/build -j && \
  ctest --test-dir tests/build --output-on-failure
```

3. Summarize all updated files (`uni_version.h`, `idf_component.yml`,
   `CHANGELOG.md`, `AUTHORS.md`). Do not run `git commit` or `git tag` unless
   explicitly requested.
