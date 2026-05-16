# SERC Mini-OS — CS 225 Assignment
## Copperbelt University | Dr Derrick Ntalasha

---

## OS Components Implemented (all 6)

1. **Process Management** — PCB struct, 5-state machine (New/Ready/Running/Waiting/Terminated), create/terminate
2. **CPU Scheduling** — FCFS, SJF, Priority, Round Robin with waiting time & turnaround time metrics
3. **Memory Management** — First-Fit allocation, block splitting/merging, fragmentation tracking, visual map
4. **IPC** — Message queue simulation (pipe-style), automatic TASK_READY signals between processes
5. **Deadlock Handling** — Banker's Algorithm safety check, allocation matrix, resource tracking
6. **File Management** — All events logged to `serc_log.txt` in real time

---

## Install on Kali Linux

### Step 1 — System dependencies
```bash
sudo apt update
sudo apt install -y git gcc make \
  libx11-dev libxcursor-dev libxrandr-dev \
  libxinerama-dev libxi-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

### Step 2 — Install Raylib from source
```bash
cd ~
git clone https://github.com/raysan5/raylib.git
cd raylib/src
make PLATFORM=PLATFORM_DESKTOP
sudo make install PLATFORM=PLATFORM_DESKTOP
sudo ldconfig
```

### Step 3 — Build
```bash
cd ~/serc_mini_os
make
```

### Step 4 — Run
```bash
./serc_mini_os
```

---

## How to Use

| Action | How |
|--------|------|
| Add a task | Processes tab → **+ Add Task** → fill form → Create |
| Watch it run | Click **TICK (+1)** repeatedly in the sidebar |
| Change scheduler | Click FCFS / SJF / Priority / Round Robin |
| View memory map | **Memory** tab |
| Check IPC messages | **IPC** tab |
| Banker's Algorithm | **Deadlock** tab |
| Read logs | **Log** tab or open `serc_log.txt` |
| Terminate a task | Processes tab → **Terminate** on any card |

> **Note:** All state is in-memory only. Restarting the program resets everything.

---

## Troubleshooting

**"cannot find -lraylib"**
```bash
sudo ldconfig
# or:
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

**Fonts look wrong / crash on font load**
The app tries DejaVu fonts (standard on Kali). If missing:
```bash
sudo apt install fonts-dejavu
```

**Running over SSH**
```bash
ssh -X user@host
./serc_mini_os
```
** Add your names :
Kim Sinzala sin 24164997