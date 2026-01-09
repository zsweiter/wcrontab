# WCrontab – Unix Crontab Emulation for Windows

**⚠️ Development Only – Not for Production Use**

WCrontab is a lightweight Unix `crontab` emulation for Windows, intended **only for development, testing, and educational purposes**.
It is **not safe for production environments**.

---

## Description

WCrontab is a C-based emulation of the Unix `crontab` system for Windows.
It provides basic cron scheduling functionality and a Windows service that simulates how a cron daemon works internally.

> 💡 For better Unix compatibility, it is recommended to run WCrontab using **Git Bash** or inside **WSL (Windows Subsystem for Linux)**.
> Running directly from CMD may have limitations.

---

## Dependencies

* GCC or Clang
* Make
* Standard C library
* Windows (native)
* Optional: Git Bash or WSL for better compatibility

---

## Build & Install

### Compile from Source

```bash
git clone https://github.com/zsweiter/wcrontab.git
cd wcrontab
make
```

### Install Binary (Optional)

```bash
make install
```

This installs the `wcrontab` binary into your user PATH.

---

## How to Use (Typical Workflow)

### 1️⃣ Install the Windows Service

```bash
wcrontab install
```

Registers WCrontab as a Windows service.

---

### 2️⃣ Start the Cron Service

```bash
wcrontab start
```

Starts the cron daemon in the background.

---

### 3️⃣ Edit the Crontab

```bash
wcrontab -e
```

Opens the crontab file in **Notepad**.
If the file does not exist, it will be created automatically.

---

### 4️⃣ List Current Jobs

```bash
wcrontab -l
```

Lists all scheduled jobs.
If no crontab exists, an empty one is created.

---

### 5️⃣ Reload After Changes

After modifying the crontab file, reload the service:

```bash
wcrontab reload
```

This applies the changes **without restarting** the service.

---

### 6️⃣ Check Logs

```bash
wcrontab logs
```

Displays execution logs for scheduled jobs and service activity.

---

### 7️⃣ Stop or Remove the Service

```bash
wcrontab stop
wcrontab uninstall
```

Stops and removes the Windows service.

---

## Supported Commands

| Command     | Description             |
| ----------- | ----------------------- |
| `-l`        | List crontab jobs       |
| `-e`        | Edit crontab            |
| `-r`        | Remove crontab          |
| `install`   | Install Windows service |
| `uninstall` | Remove Windows service  |
| `start`     | Start cron service      |
| `stop`      | Stop cron service       |
| `pause`     | Pause service           |
| `resume`    | Resume service          |
| `reload`    | Reload crontab          |
| `logs`      | View execution logs     |

---

## Notes on Security

* Job commands may be executed using system-level APIs.
* This project **does not sandbox commands**.
* Do not run as Administrator.
* Intended for **local testing only**.

---

## Warning

This software is provided **as-is**, without warranties or guarantees.
It is intended strictly for **development and learning purposes**.
