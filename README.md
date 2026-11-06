# Windows IPC & Multithreading Platform (Placar Informativo)

[![C](https://img.shields.io/badge/Language-C99%20%2F%20C%2B%2B-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Windows API](https://img.shields.io/badge/Platform-Windows%20Win32%20API-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://learn.microsoft.com/en-us/windows/win32/)
[![Visual Studio](https://img.shields.io/badge/IDE-Visual%20Studio%202022-5C2D91?style=for-the-badge&logo=visualstudio&logoColor=white)](https://visualstudio.microsoft.com/)
[![License](https://img.shields.io/badge/Academic-ISEC_LEI-green?style=for-the-badge)](#)

A high-performance, concurrent distributed application built strictly on native **Win32 API** in **C/C++**, developed for the **Operating Systems II (SO2)** course in Computer Engineering at **ISEC (Instituto Superior de Engenharia de Coimbra)**.

The project demonstrates advanced low-level systems programming, including **Inter-Process Communication (IPC)** via bidirectional Named Pipes and Shared Memory, process synchronization with Mutexes, Events, and Critical Sections, asynchronous Waitable Timers, Windows Registry interaction, and a native Win32 GUI client.

---

## 🏛️ System Architecture

The ecosystem consists of three decoupled components communicating through native Windows IPC:

```
                  ┌──────────────────────┐
                  │    Windows Registry  │
                  │  (HKCU\Software\...) │
                  └──────────▲───────────┘
                             │ (Fallback Config)
┌────────────────┐           │           ┌────────────────┐
│     Placar     │◄──────────┼──────────►│    Central     │
│  (CLI Client)  │   Named Pipes (IPC)   │ (Server Core)  │
└────────────────┘ (Bidirectional Msg)   └───────┬────────┘
                                                 │
                                                 │ Shared Memory (SHM)
                                                 │ + Mutex & Win32 Event
                                                 ▼
                                         ┌────────────────┐
                                         │    Monitor     │
                                         │  (Win32 GUI)   │
                                         └────────────────┘
```

---

## 🚀 Key Modules & Technical Implementation

### 1. `Central` (Core Server Engine)
* **Named Pipe Server:** Operates in bidirectional message mode (`PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE`), dynamically accepting and managing connections from multiple `Placar` clients.
* **Concurrency & Safety:** Multithreaded request processing secured with **Critical Sections** to prevent race conditions during alert mutations and ID assignments.
* **Shared Memory Producer:** Exposes active system state to external monitors using memory-mapped files (`CreateFileMapping` / `MapViewOfFile`), strictly avoiding raw pointers in shared memory.
* **Signaling:** Fires Win32 Events to notify attached monitors with **zero CPU polling**.

### 2. `Placar` (Command-Line Client)
* **Dual-Thread Execution:**
  * **Input Thread:** Listens for interactive user commands (`ligar`, `desligar`, etc.).
  * **Worker Thread:** Listens asynchronously for server broadcast notifications and alerts.
* **Configuration Persistence:** Reads/writes default Named Pipe parameters directly from the **Windows Registry** (`HKCU\Software\TrabSO2\NPIPE`).
* **Waitable Timers:** Uses Windows kernel Waitable Timers (`CreateWaitableTimer`, `SetWaitableTimer`) for deterministic expiration of alert displays.

### 3. `Monitor` (Native Win32 GUI Client)
* **Zero-Polling Consumer:** Maps the Central server's shared memory segment and blocks passively waiting on a Win32 Event, consuming negligible CPU when idle.
* **Graphical Features:** Win32 dialogs (Configuration, About), menu bar, and dynamic pagination using `Page Up` / `Page Down` keys for large alert queues.

---

## 🛠️ Build & Execution Instructions

### Requirements
* **Operating System:** Windows 10 / 11
* **Compiler / Toolchain:** MSVC (Visual Studio 2022 with C/C++ Desktop Development workload)

### Compiling
1. Clone the repository:
   ```bash
   git clone https://github.com/7uppx/windows-ipc-multithreading.git
   cd windows-ipc-multithreading
   ```
2. Open `TrabalhoPraticoSo2.sln` in Visual Studio.
3. Select **x64** and **Release** (or **Debug**).
4. Build the solution (**Ctrl + Shift + B**).

### Execution Flow
1. Start the server:
   ```cmd
   central.exe \\.\pipe\placar_pipe
   ```
2. Launch one or more display clients:
   ```cmd
   placar.exe \\.\pipe\placar_pipe
   ```
3. Run the GUI monitor:
   ```cmd
   monitor.exe
   ```

---

## 👨‍💻 Author & Academic Context

* **João Bernardo Graça**

Developed as part of the **Degree in Computer Engineering (LEI)** - *Application Development (DA) branch* at **ISEC (Instituto Superior de Engenharia de Coimbra)**.

