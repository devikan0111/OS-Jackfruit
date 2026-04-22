# Mini Container Runtime (Jackfruit OS Project)


## Team Details
<div align="left">

| **Name**                 | **SRN**          |
|--------------------------|------------------|
| Dabbugunta Venya Anand   | PES1UG24AM074    |
| Devika N                 | PES1UG24AM080    |

</div>


##  Project Overview
This project implements a **lightweight Linux container runtime in C** with a long-running supervisor and kernel-level monitoring.

The system is capable of:
- Managing multiple containers concurrently
- Providing process isolation using Linux namespaces
- Handling inter-process communication (IPC)
- Implementing bounded-buffer logging
- Enforcing memory limits using a kernel module
- Demonstrating Linux scheduling behavior

---

##  Features

### Container Isolation
- Uses `clone()` system call
- Implements:
  - **PID Namespace (`CLONE_NEWPID`)**
  - **Mount Namespace (`CLONE_NEWNS`)**
- Ensures:
  - Containers cannot see host processes
  - Separate process trees and mount points

---

###  Supervisor (Control Plane)
- Long-running parent process
- Responsible for:
  - Spawning containers
  - Managing lifecycle (START, STOP, CLEANUP)
  - Maintaining metadata registry
- Prevents zombie processes using `waitpid()`

---

### Inter-Process Communication (IPC)
- Uses **Unix Domain Sockets**
  - Path: `/tmp/mini_runtime.sock`
- Enables communication between:
  - CLI (client)
  - Supervisor (server)

---

###  Bounded-Buffer Logging
- Implements **producer-consumer model**
- Prevents system blocking due to excessive logs
- Uses:
  - **Mutexes** → for mutual exclusion
  - **Condition Variables** → for synchronization

---

###  Metadata Tracking
- Maintains:
  - Container ID
  - Host PID
  - State
  - Memory limits
- Acts as the **"brain" of the supervisor**

---

### Memory Management (Kernel Module)
- Custom kernel module: `monitor.ko`
- Implements:
  - **Soft Limit** → Warning threshold
  - **Hard Limit** → Process termination (SIGKILL)
- Ensures system stability under heavy memory usage

---

###  Scheduling Analysis
- Demonstrates Linux **Completely Fair Scheduler (CFS)**
- Uses `nice` values to control priority
- Observations:
  - High-priority processes get more CPU time
  - Low-priority processes still execute (fairness)

---

##  Screenshots & Explanation

###  Screenshot 1 – Container Initialization
- Supervisor starts successfully
- Multiple containers (ALPHA, BETA) created
- Each has isolated PID and mount namespace


Clean Workspace

  <img width="940" height="69" alt="image" src="https://github.com/user-attachments/assets/7137768d-4465-485f-9b57-f4c7e6960961" />

Alpha and Beta have the Alpine Linux structure inside them

<img width="940" height="96" alt="image" src="https://github.com/user-attachments/assets/013c82cd-c30d-4a94-846a-b4585077d48d" />


Supervisor process initialized in Terminal 1, successfully spawning isolated container processes with unique PIDs and namespaces.

<img width="940" height="72" alt="image" src="https://github.com/user-attachments/assets/fd7c6424-c923-44d0-987c-54087aa95e86" />

Supervisor initialized using rootfs-base. The process enters a listening state, ready to manage multiple concurrent containers

<img width="940" height="63" alt="image" src="https://github.com/user-attachments/assets/c49238c4-b525-46a4-afb4-ae3b2a4641d5" />


<img width="940" height="27" alt="image" src="https://github.com/user-attachments/assets/6b97e48a-5245-43d3-9858-ea180bb47166" />

<img width="940" height="78" alt="image" src="https://github.com/user-attachments/assets/41fd6393-28e4-4450-b364-dd2116fab1b3" />

Two containers running under one supervisor process
The presence of two distinct containers (ALPHA and BETA) proves that use of the clone() system call with CLONE_NEWPID and CLONE_NEWNS is successfully creating isolated process trees and mount points. This ensures that processes inside the container cannot see or interfere with processes on the host


---


### Screenshot 2 – CLI & IPC


<img width="940" height="90" alt="image" src="https://github.com/user-attachments/assets/3af8fe8e-2682-4d2b-bd22-718d7323ed68" />

<img width="940" height="32" alt="image" src="https://github.com/user-attachments/assets/f6f4757c-ac8d-4744-913c-d9bfeaf818a3" />


This screenshot confirms the communication layer between the user and the supervisor. It shows a command being issued from the client terminal which is then intercepted by the supervisor via a Unix Domain Socket (/tmp/mini_runtime.sock). The log proves that the Inter-Process Communication (IPC) protocol is correctly handling requests in real-time.

---

### Screenshot 3 – Logging SystemBounded-buffer logging:

Log file contents captured through the logging pipeline, and evidence of the pipeline operating
To prevent the supervisor from being overwhelmed by high-volume output from containers, a Bounded-Buffer mechanism is employed. This architecture prevents "blocking" where a container would pause execution if its output buffer was full, ensuring that the engine remains responsive even under heavy logging workloads.

 The screenshots of the log files demonstrate that even when processes are running inside restricted namespaces, their output is successfully piped back to the host and stored for auditing.

<img width="940" height="287" alt="image" src="https://github.com/user-attachments/assets/860dc7e8-7079-42e3-a507-375756eec624" />

**STDERR Error**:

<img width="940" height="180" alt="image" src="https://github.com/user-attachments/assets/cb83657d-42be-4eab-948b-173236388da5" />



---

### Screenshot 4 – Metadata Tracking

The metadata registry is the "brain" of the supervisor.

The ps table displays active containers, their host PIDs, current states, and assigned memory thresholds. This confirms that the supervisor is correctly maintaining a metadata registry of all running instances and their resource constraints.

<img width="940" height="114" alt="image" src="https://github.com/user-attachments/assets/e23d34e7-0e66-4cea-89b4-3282cd0e2678" />

<img width="936" height="72" alt="image" src="https://github.com/user-attachments/assets/a49f2b88-3f01-48c5-a178-cb21ef126d13" />



---

### Screenshot 5 & 6 – Memory Limits

Soft-limit warning and hard limit warning
dmesg or log output showing a soft-limit warning event for a container.

**Workload Setup**:


**TERMINAL 1**


<img width="809" height="55" alt="image" src="https://github.com/user-attachments/assets/40190e1b-c869-4ee3-aecc-5f97fafa826e" />


<img width="940" height="94" alt="image" src="https://github.com/user-attachments/assets/2aba1da2-d56e-4ae8-a0b6-3e73f32fa9ac" />


<img width="789" height="117" alt="image" src="https://github.com/user-attachments/assets/c8fa9c2f-019c-45fd-91c9-248fdf110ba7" />


<img width="940" height="82" alt="image" src="https://github.com/user-attachments/assets/7c3f5c33-f312-456e-8272-93b55dac647b" />


<img width="940" height="88" alt="image" src="https://github.com/user-attachments/assets/4fafcdde-07e8-4d3d-b58f-48b93273db40" />



**TERMINAL 2**

**Multi-Workload Setup**

<img width="940" height="133" alt="image" src="https://github.com/user-attachments/assets/5b1e7dc6-7303-4306-ac63-738bc5383130" />


<img width="940" height="183" alt="image" src="https://github.com/user-attachments/assets/f3a9f89a-611a-45d7-b06a-ae378e02bfc9" />

Workload Initialization. Deployment of CPU-intensive and I/O-intensive workloads to observe differential treatment by the Linux scheduler.


Soft-limit warning and hard limit warning:


<img width="940" height="305" alt="image" src="https://github.com/user-attachments/assets/f42ba178-f5d4-4aa8-b213-552226059803" />


<img width="940" height="301" alt="image" src="https://github.com/user-attachments/assets/624296c0-6338-4360-bd4e-eb3069aaa464" />


<img width="1043" height="612" alt="image" src="https://github.com/user-attachments/assets/18dd1456-e96f-454e-adbe-4d07587a802d" />


 
Kernel-level soft-limit enforcement. The dmesg output confirms that the monitor.ko module has detected the container exceeding its soft memory threshold and has issued a kernel warning without terminating the process.

---

###  Screenshot 7 – Scheduling

**LOW PRIORITY**

<img width="975" height="196" alt="image" src="https://github.com/user-attachments/assets/071b43e3-dc2a-46ab-b19f-bfec62cd7d07" />


**HIGH PRIORITY**

<img width="975" height="148" alt="image" src="https://github.com/user-attachments/assets/d4844297-92b8-430d-b78e-052dc32737ce" />



<img width="975" height="696" alt="image" src="https://github.com/user-attachments/assets/26fcef1e-0a35-4c8b-a996-219c21c0569c" />



These screenshots analyze the differential treatment of workloads by the Linux Completely Fair Scheduler (CFS). By deploying two cpu_hog processes with different "Nice" values, the top output clearly shows that the high-priority container (lower nice value) is allocated a significantly larger percentage of CPU cycles compared to the low-priority container, proving successful resource prioritization.

**CPU**:

<img width="940" height="581" alt="image" src="https://github.com/user-attachments/assets/8bc15b9a-7e87-4b69-9adf-006d57e4ad68" />

**I/O**:

<img width="820" height="531" alt="image" src="https://github.com/user-attachments/assets/a0426c3c-71c8-4c09-9e6d-3b20e0e51485" />




---

### Screenshot 8 – Cleanup

**Cleanup**:

<img width="940" height="78" alt="image" src="https://github.com/user-attachments/assets/5b3bf0a5-d3a3-4bc3-8372-cda076e4ee77" />

<img width="940" height="107" alt="image" src="https://github.com/user-attachments/assets/46c62eb9-b31d-4a6b-b625-43cba848721b" />

When a SIGINT is sent to the supervisor, the process triggers its cleanup routine. It closes the Unix Domain Sockets (/tmp/mini_runtime.sock), flushes the remaining logs from the bounded buffer to disk, and ensures that all child container processes are properly reaped to avoid leaving zombie processes in the host system.

---



**ENGINEERING ANALYSIS**:

**Isolation Mechanisms**:

The runtime achieves isolation by leveraging Linux namespaces, which wrap global system resources in an abstraction that makes it appear to the process that it has its own isolated instance of the resource.

•	PID Namespace: By using CLONE_NEWPID, the kernel ensures that the first process in the container becomes PID 1. This prevents the container from seeing or signaling processes on the host.

•	Mount Namespace and Chroot: The project uses CLONE_NEWNS and chroot to re-root the process into a specific directory (like the Alpine rootfs). At the kernel level, this manipulates the VFS (Virtual File System) layer so the process cannot traverse upward to the host’s sensitive directories.

**Supervisor and Process Lifecycle**:
A long-running supervisor is the "Control Plane" of the container.

•	Parent-Child Relationship: The supervisor acts as the stable anchor. When a child process terminates, it becomes a "zombie" until the parent calls waitpid(). This reaping process is essential to prevent the system's process table from filling up.

•	The supervisor translates high-level commands (like "STOP") into low-level kernel signals (SIGTERM or SIGKILL). It acts as a bridge between the user's intent and the process's lifecycle, ensuring metadata (like current state and runtime duration) is tracked accurately from spawn to exit.


**IPC, Threads, and Synchronization**:

•	Bounded-Buffer Logging: The logging pipeline utilizes a shared buffer between the supervisor's reader threads and the writer logic.

•	Synchronization Choice: We use Mutexes for the container metadata list to prevent race conditions during simultaneous START and PS commands. For the bounded buffer, Condition Variables are used to handle the producer-consumer problem. This ensures that a thread "sleeps" if the buffer is empty, rather than "spinning" and wasting CPU cycles (busy-waiting).

**Memory Management and Enforcement**:

•	Soft vs. Hard Limits: A Soft Limit is an advisory threshold for resource "pressure" (allowing for bursts), while a Hard Limit is an absolute physical constraint to protect system stability.

•	Kernel-Space Enforcement: Enforcement must live in kernel space (via the monitor.ko module) because user-space programs can be "paused" or "blocked." A kernel module can use hardware-level timers and high-priority interrupts to guarantee that a "Super-Polluter" or memory-hungry process is killed (SIGKILL) the millisecond it becomes a threat to the OS.

**Scheduling Behavior**:

•	By adjusting "Nice" values, we manipulate the virtual runtime of a process. A process with a higher priority (lower nice) is granted more time slices.

•	Linux aims for fairness (no process starves), but our project exercises priority-based scheduling. The results show that the OS prioritizes throughput for the high-priority container while maintaining enough responsiveness so that the low-priority container eventually completes its task. This proves the OS balances the need for speed with the fundamental requirement that all processes eventually make progress


