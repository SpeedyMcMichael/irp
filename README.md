# irp (Internet Relay Pipeline)

A minimalist, suckless IRC client written in **C** with **ncurses**. It features modern SASL authentication and (later) TLS.

## Features
* **Minimalist UI**: Gray-on-blue ncurses interface with a 1000-line scrollback pad.
* **SASL PLAIN**: Internal state machine for authenticating with modern networks like Libera.Chat.
* **Suckless Build**: Uses Meson/Ninja for fast, reproducible compilation.

---

## Prerequisites

* **Compiler**: `gcc` or `clang`
* **Libraries**: `ncursesw` (with wide-character support)
* **Build System**: `meson` and `ninja`
* **Tunneling**: `socat` (optional, recommended for security)

---

## Build Instructions

1.  **Configure the build directory**:
    ```bash
    meson setup build
    ```
2.  **Compile the binary**:
    ```bash
    meson compile -C build
    ```
3.  **Install (Optional)**:
    ```bash
    sudo meson install -C build
    ```

---

## Deployment Workflow
TODO: integrate `openssl` into `irp`

TEMPORARY SECURITY MESURES:

### 1. Establish the Secure Tunnel
Run `socat` in the background to bridge a local port to Libera's SSL port:
```bash
socat TCP-LISTEN:16667,fork,reuseaddr OPENSSL:irc.libera.chat:6697 &
```

### 2. Configure & Run
Ensure your `config.h` points to `127.0.0.1` on port `16667`. Then launch:
```bash
./build/irp
```

---

## Usage & Keybindings

| Key | Action |
| :--- | :--- |
| **PageUp** | Scroll back through history |
| **PageDown** | Scroll forward to recent messages |
| **Enter** | Send message or execute command |
| **/j #channel** | Join a specific channel |
| **/msg <nick> <msg>** | Send a private message |
| **/q** | Gracefully quit the pipeline |

---

## Configuration

All settings (colors, nicks, SASL credentials) are managed in `src/config.h`. 

> **Note**: If you receive a `904 SASL authentication failed` notice but still connect, ensure your `PASS` matches your registered NickServ account. If you aren't registered yet, Libera will let you in as an unidentified user.

TODO/CONTRIBUTOR GOALS:
* Add and integrate `openssl`
* Add to AUR
* Iron out kinks
