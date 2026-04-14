# OS-Jackfruit — Multi-Container Runtime

A lightweight Linux container runtime in C with a long-running supervisor and a kernel-space memory monitor.

## Team Information

- **Simran Joshi** — PES1UG24CS455
- **Shubashitha Gowtham** — PES1UG24CS450

---

## 1. Project Overview

OS-Jackfruit is a multi-container runtime built in C. It uses a long-running user-space supervisor (`engine`) and a kernel module (`monitor`) to:

- launch and manage isolated container-like workloads
- maintain per-container metadata
- capture stdout/stderr through a logging pipeline
- enforce memory limits using a kernel-space monitor
- demonstrate Linux scheduling behavior through controlled experiments

The repository contains:

- `engine.c` — user-space runtime and supervisor
- `monitor.c` — kernel-space memory monitor
- `monitor_ioctl.h` — shared ioctl definitions
- `cpu_hog.c`, `io_pulse.c`, `memory_hog.c` — test workloads
- `Makefile` — build flow
- `environment-check.sh` — VM preflight check

---

## 2. Prerequisites

Use an **Ubuntu 22.04 or 24.04 VM**.

Requirements:

- Secure Boot **OFF**
- WSL is **not supported**
- `build-essential`
- matching Linux headers for the running kernel

Install dependencies:

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
```

---

## 3. Root Filesystem Setup

Create a base rootfs and per-container writable copies:

```bash
mkdir -p rootfs-base
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz -C rootfs-base

cp -a ./rootfs-base ./rootfs-alpha
cp -a ./rootfs-base ./rootfs-beta
```

Do **not** commit `rootfs-base/` or any `rootfs-*` directories.

If you need helper binaries inside a container, copy them into the container rootfs before launch:

```bash
cp ./memory_hog ./rootfs-alpha/
cp ./cpu_hog ./rootfs-beta/
```

---

## 4. Environment Check

Run the provided preflight script from the `boilerplate/` directory:

```bash
cd boilerplate
chmod +x environment-check.sh
sudo ./environment-check.sh
```

Fix any issues before proceeding.

---

## 5. Build Instructions

From `boilerplate/`:

```bash
make
```

For the GitHub Actions smoke check:

```bash
make -C boilerplate ci
```

The CI-safe build is only a compilation check for user-space binaries and usage output. It does not test kernel module loading or container execution.

---

## 6. Kernel Module

Load the memory monitor:

```bash
sudo insmod monitor.ko
```

Verify that the character device exists:

```bash
ls -l /dev/container_monitor
```

If needed, allow access during local testing:

```bash
sudo chmod 666 /dev/container_monitor
```

Unload the module when finished:

```bash
sudo rmmod monitor
```

---

## 7. Runtime Usage

The runtime is designed around a long-running supervisor and short-lived CLI commands.

### Supervisor

Start the supervisor once:

```bash
sudo ./engine supervisor ./rootfs-base
```

### Start a container

Launch a container with a writable rootfs copy and resource limits:

```bash
sudo ./engine start alpha ./rootfs-alpha /bin/sh --soft-mib 48 --hard-mib 80
sudo ./engine start beta ./rootfs-beta /bin/sh --soft-mib 64 --hard-mib 96
```

### Run a command and wait for completion

```bash
sudo ./engine run alpha ./rootfs-alpha /bin/sh --soft-mib 48 --hard-mib 80
```

### List containers

```bash
sudo ./engine ps
```

### View logs

```bash
sudo ./engine logs alpha
```

### Stop a container

```bash
sudo ./engine stop alpha
```

---

## 8. Memory Monitor Behavior

The kernel monitor tracks container processes through `/dev/container_monitor` and enforces per-process RSS limits.

### Soft limit
When a process exceeds the soft limit for the first time, the monitor logs a warning.

### Hard limit
When a process exceeds the hard limit, the monitor sends `SIGKILL` and removes the entry.

### Kernel-side responsibilities

- register host PIDs via ioctl
- keep monitored processes in a linked list
- protect shared state with a mutex
- check RSS periodically with a timer
- remove exited or killed processes
- free all remaining entries on module unload

---

## 9. Workload Programs

The boilerplate includes three helper workloads:

- `memory_hog` — memory-heavy workload for monitor testing
- `cpu_hog` — CPU-bound workload for scheduler experiments
- `io_pulse` — I/O-heavy workload for comparison runs

Example background run:

```bash
./memory_hog > /dev/null 2>&1 &
./cpu_hog > /dev/null 2>&1 &
./io_pulse > /dev/null 2>&1 &
```

---

## 10. Scheduler Experiments

The project includes a small scheduling study to compare Linux behavior under different priorities or workload mixes.

### Example experiment: different nice values

```bash
for i in {1..6}; do ./cpu_hog > /dev/null 2>&1 & done
sleep 1
PIDS=($(pgrep cpu_hog | head -n 2))
sudo renice -10 -p "${PIDS[0]}"
renice 10 -p "${PIDS[1]}"
top
```

### Example experiment: CPU-bound vs I/O-bound

Run one `cpu_hog` and one `io_pulse` together, then compare responsiveness and CPU share.

Record:

- process IDs
- nice values
- CPU percentage
- completion time or responsiveness
- what changed when priorities changed

---

## 11. Proof of Execution

### 1. Device Creation
The kernel module successfully creates the character device `/dev/container_monitor`, which is used for communication between user space and kernel space.

<img width="745" height="72" alt="image" src="https://github.com/user-attachments/assets/c44b5d24-be30-41cf-9818-d75b15953a43" />


---

### 2. Process Execution and Registration
A memory-intensive workload (`memory_hog`) is executed in the background and its PID is captured. The process is then registered with the kernel module using the `engine` interface.

<img width="753" height="143" alt="image" src="https://github.com/user-attachments/assets/fffb565c-4722-4ce7-9c85-5b08b487c2b5" />


---

### 3. Soft and Hard Limit Enforcement
The kernel module correctly monitors memory usage and enforces limits:

- A **soft limit warning** is logged when memory exceeds the soft threshold.
- A **hard limit kill** is triggered when memory exceeds the hard threshold.

<img width="748" height="166" alt="image" src="https://github.com/user-attachments/assets/307c23f7-9a52-4399-9ac6-d76f4ade851e" />

---

### 4. Process Termination
After exceeding the hard limit, the monitored process is terminated by the kernel module, demonstrating correct enforcement.

<img width="754" height="94" alt="image" src="https://github.com/user-attachments/assets/e01eba4d-131c-405c-a305-e841604ffe29" />

---

### 5. Multi-Process Monitoring
Multiple processes can be registered simultaneously with different container IDs and limits. The kernel module independently tracks and enforces limits for each process.

<img width="748" height="278" alt="image" src="https://github.com/user-attachments/assets/74876794-048c-45e8-8b02-db60f20e0e9b" />

---

### 6. Scheduler Behavior
CPU scheduling behavior is demonstrated using multiple `cpu_hog` processes with different nice values.

- Process with **nice = -10** receives higher CPU share  
- Process with **nice = 10** receives lower CPU share  

<img width="749" height="534" alt="image" src="https://github.com/user-attachments/assets/7b408696-b664-4d01-8d98-32964e44be1c" />

---

### 7. Clean Module Teardown
The kernel module is successfully unloaded, ensuring that all internal data structures are cleaned up and no residual state remains.

<img width="541" height="32" alt="image" src="https://github.com/user-attachments/assets/0d0d2124-8eaf-40cc-979a-49f1305c2a3e" />


---

## 12. Engineering Analysis

### 12.1 Isolation Mechanisms

The runtime isolates workloads using Linux namespaces and a separate root filesystem per container. PID namespaces isolate process IDs, mount namespaces isolate the filesystem view, and a fresh rootfs limits what the process can see. The host kernel is still shared; containers are lightweight because they reuse the same kernel rather than booting a separate one.

### 12.2 Supervisor and Process Lifecycle

A long-running supervisor simplifies container management because it owns metadata, child reaping, signal handling, and cleanup. It can launch, stop, and inspect containers while keeping the lifecycle state in one place.

### 12.3 IPC, Threads, and Synchronization

The project uses separate IPC paths for control and logging. Shared data structures must be protected against races because multiple threads and processes can access them at once. Mutexes are used for shared state that is accessed in process context, and producer/consumer synchronization is used for log buffering.

### 12.4 Memory Management and Enforcement

RSS measures resident physical memory used by a process. Soft limits warn first; hard limits terminate the process. Enforcement is implemented in kernel space so that a workload cannot bypass it from user space.

### 12.5 Scheduling Behavior

Linux scheduling is demonstrated by running competing workloads with different nice values or different workload types. Lower nice values receive more favorable scheduling, while I/O-bound and CPU-bound tasks behave differently under contention.

---

## 13. Design Decisions and Tradeoffs

### Namespace and rootfs design
- **Choice:** separate rootfs copy per container
- **Tradeoff:** more disk usage
- **Why:** simpler isolation and safer mutation boundaries

### Supervisor architecture
- **Choice:** one long-running supervisor
- **Tradeoff:** more code in one process
- **Why:** easier lifecycle tracking, metadata management, and signal handling

### Logging pipeline
- **Choice:** pipes + bounded buffer + consumer thread
- **Tradeoff:** more synchronization complexity
- **Why:** prevents log loss and keeps stdout/stderr collection structured

### Kernel memory monitor
- **Choice:** kernel module with ioctl registration
- **Tradeoff:** more privilege and debugging complexity
- **Why:** reliable enforcement that user-space processes cannot evade

### Synchronization choice
- **Choice:** mutex for shared monitored-list state
- **Tradeoff:** sleeping lock rather than lock-free access
- **Why:** safe and simple in process context

---

## 14. Cleanup

The runtime and kernel module are designed to shut down cleanly. Containers are reaped, logs are closed, memory is released, and module state is removed on unload.

--- 

## 15. Verification

A complete run should show:

- the supervisor starting successfully
- containers launching and exiting cleanly
- logs being captured
- soft-limit warnings in kernel logs
- hard-limit kills when limits are exceeded
- clean module unload with no leftover state

---

## 16. Clean Build and Teardown Commands

```bash
make
sudo insmod monitor.ko
ls -l /dev/container_monitor
sudo ./engine supervisor ./rootfs-base
sudo ./engine ps
sudo ./engine stop alpha
sudo rmmod monitor
```

---


