# Contributing to irp

Thanks for your interest in contributing to **irp** (Internet Relay Pipeline).

This project follows a **suckless** philosophy: minimal code, minimal dependencies, and clarity over cleverness. Please keep this in mind before submitting patches — features that bloat the binary, add runtime dependencies, or duplicate what the Unix pipeline already does well will likely be rejected.

## Table of Contents
- [Project Philosophy](#project-philosophy)
- [Reporting Bugs](#reporting-bugs)
- [Suggesting Features](#suggesting-features)
- [Development Setup](#development-setup)
- [Coding Style](#coding-style)
- [Commit Messages](#commit-messages)
- [Submitting a Merge Request](#submitting-a-merge-request)
- [Testing](#testing)

---

## Project Philosophy

Before contributing, please understand the design constraints:

1. **Zero-bloat**: The compiled binary should remain under ~100KB. No OpenSSL, GnuTLS, or other heavy linkage.
2. **Pipeline-first**: If a problem can be solved with `socat`, `stunnel`, or another Unix tool, it should be — not inside `irp`.
3. **C + ncurses only**: No scripting language bindings, no plugin systems, no config files at runtime. All configuration lives in `src/config.h` and is set at compile time.
4. **Readable C**: Prefer clear, short functions over abstraction. Avoid macros that hide control flow.

Patches that violate these principles will not be merged, even if technically correct.

---

## Reporting Bugs

Open an issue on GitLab with:

- **Title**: Short and specific (e.g., `SASL PLAIN fails on Libera with non-ASCII password`)
- **Environment**: OS, compiler version, ncurses version, `socat` version (if relevant)
- **Steps to reproduce**: Including the exact `socat` command used for tunneling
- **Expected vs. actual behavior**
- **Logs**: Output from `irp` and, if applicable, the IRC server's numeric replies (e.g., `904`, `462`)

Please check existing issues first to avoid duplicates.

---

## Suggesting Features

Open an issue labeled `enhancement`. Be prepared to justify why the feature belongs in `irp` rather than in a wrapper script or external tool. Good candidates:

- Bugfixes to the SASL state machine
- IRCv3 capability negotiation improvements
- ncurses rendering fixes (resize handling, UTF-8 edge cases)
- Reduced binary size or memory footprint

Likely-rejected suggestions:

- Built-in TLS (use `socat`/`stunnel`)
- Scripting/plugin support
- Logging to disk (pipe `irp` output to `tee` instead)
- DCC, CTCP file transfer, or other legacy bloat

---

## Development Setup

1. **Fork** the repository on GitLab.
2. **Clone** your fork:
   ```bash
   git clone https://gitlab.com/<your-username>/irp.git
   cd irp
   ```
3. **Install dependencies** (Debian/Ubuntu example):
   ```bash
   sudo apt install gcc meson ninja-build libncursesw5-dev socat
   ```
4. **Configure and build**:
   ```bash
   meson setup build
   meson compile -C build
   ```
5. **Create a branch** for your work:
   ```bash
   git checkout -b fix/sasl-edge-case
   ```

---

## Coding Style

- **Indentation**: Tabs, width 8 (suckless convention).
- **Braces**: K&R style — opening brace on same line for functions and control structures.
- **Line length**: Aim for 80 columns; hard limit 100.
- **Naming**:
  - Functions: `lower_snake_case` (e.g., `sasl_send_authenticate`)
  - Constants/macros: `UPPER_SNAKE_CASE`
  - Static globals: prefixed with module name (e.g., `ui_pad`, `irc_sock`)
- **Headers**: Keep `config.h` as the single source of truth for user-tunable values.
- **No `malloc` in hot paths**: Prefer fixed-size buffers where IRC line limits (512 bytes) make this trivial.
- **Error handling**: Check every `read`, `write`, `recv`, `send`. Fail loudly with a clear message via `endwin()` + `fprintf(stderr, ...)` + `exit(EXIT_FAILURE)`.

Run `clang-format` only if a `.clang-format` file is present in the repo. Otherwise match surrounding code.

---

## Commit Messages

Use the imperative mood and a conventional prefix:

- `feat: add CAP LS 302 negotiation`
- `fix: handle truncated SASL AUTHENTICATE response`
- `refactor: split irc.c into irc_proto.c and irc_io.c`
- `docs: clarify socat tunneling in README`
- `style: align struct members in config.h`
- `build: bump meson minimum to 0.60`

**Format:**

```
<type>: <short summary, <72 chars>

Optional longer body explaining *why* the change is needed,
not *what* it does (the diff already shows that).

Closes #<issue-number>
```

---

## Submitting a Merge Request

1. Ensure the project **builds cleanly** with `-Wall -Wextra -Werror`.
2. Test against at least one real IRC network (Libera.Chat recommended) via `socat`.
3. Confirm the binary size hasn't ballooned:
   ```bash
   size build/irp
   ```
4. Update `README.md` if you change user-facing behavior.
5. Push your branch and open