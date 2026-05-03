# CSE 320 — Lab 5: TCP Tahoe Congestion Control & Distance-Vector Routing

> **Algorithm:** TCP Tahoe &nbsp;|&nbsp; **Formula:** (20220808001 + 20220808006) mod 3 = **0** → Tahoe  
> **Students:** 20220808001 Serhat Buğra Tana · 20220808006 Alperen Ulukaya

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Repository Structure](#2-repository-structure)
3. [Prerequisites](#3-prerequisites)
4. [Building](#4-building)
5. [Running — TCP Tahoe Simulator](#5-running--tcp-tahoe-simulator)
6. [Running — 6-Node Routing System](#6-running--6-node-routing-system)
7. [Algorithm Details](#7-algorithm-details)
8. [Network Topology](#8-network-topology)
9. [Configuration File Format](#9-configuration-file-format)
10. [Sample Output](#10-sample-output)

---

## 1. Project Overview

This lab implements two related networking concepts:

| Component | File | Description |
|-----------|------|-------------|
| **TCP Tahoe Simulator** | `tcp_sim.c` | Simulates TCP Tahoe congestion control over 50 RTTs with injected loss events (Timeout & Triple Duplicate ACK) |
| **6-Node Routing** | `node.c` | A distance-vector routing daemon — 6 nodes (A–F) communicate over UDP and forward messages via Dijkstra-computed shortest paths |

---

## 2. Repository Structure

```
lab5/
├── tcp_sim.c   # TCP Tahoe congestion control simulator
├── node.c      # 6-node UDP routing daemon (Dijkstra)
├── A.conf      # Node A config  (port 5001, neighbors: B C D F)
├── B.conf      # Node B config  (port 5002, neighbors: A D E)
├── C.conf      # Node C config  (port 5003, neighbors: A D E)
├── D.conf      # Node D config  (port 5004, neighbors: A B C)
├── E.conf      # Node E config  (port 5005, neighbors: B C)
├── F.conf      # Node F config  (port 5006, neighbor:  A)
├── Makefile
└── README.md
```

---

## 3. Prerequisites

| Platform | Requirement |
|----------|-------------|
| **Windows** | [MinGW-w64](https://www.mingw-w64.org/) or [MSYS2](https://www.msys2.org/) with `gcc` in `PATH` |
| **Linux / macOS** | `gcc` (any recent version) |

Verify your compiler:

```bash
gcc --version
```

---

## 4. Building

### Using Make (recommended)

```bash
make
```

This produces two binaries: `tcp_sim` (or `tcp_sim.exe`) and `node` (or `node.exe`).

### Manual compilation

```bash
# TCP simulator
gcc -Wall -Wextra -o tcp_sim tcp_sim.c

# Routing node (Windows — link Winsock)
gcc -Wall -Wextra -o node node.c -lws2_32

# Routing node (Linux / macOS)
gcc -Wall -Wextra -o node node.c
```

### Clean build artifacts

```bash
make clean
```

---

## 5. Running — TCP Tahoe Simulator

```bash
# Linux / macOS
./tcp_sim tahoe

# Windows (MinGW / MSYS2 terminal)
./tcp_sim.exe tahoe

# Argument is optional — defaults to Tahoe
./tcp_sim
```

### What it does

The simulator steps through **50 RTTs** and prints `cwnd` and `ssthresh` at each round.  
Three loss events are injected automatically:

| RTT | Event | Tahoe Reaction |
|-----|-------|---------------|
| **8** | **Timeout** | `ssthresh = cwnd/2`, `cwnd = 1 MSS`, restart Slow Start |
| **18** | **Triple Duplicate ACK** | Same as Timeout in Tahoe: `ssthresh = cwnd/2`, `cwnd = 1 MSS` |
| **35** | **Timeout** | `ssthresh = cwnd/2`, `cwnd = 1 MSS`, restart Slow Start |

---

## 6. Running — 6-Node Routing System

Open **6 separate terminal windows** and run one command in each:

```bash
# Terminal 1
./node A.conf

# Terminal 2
./node B.conf

# Terminal 3
./node C.conf

# Terminal 4
./node D.conf

# Terminal 5
./node E.conf

# Terminal 6
./node F.conf
```

Each node starts up, computes its routing table with Dijkstra, and begins listening on its UDP port.

### Sending a message

In the **Node A terminal**, type:

```
send D hello_from_A_to_D
```

Expected output across terminals:

```
[A] Forwarding message from A to D, next hop B (port 5002)
[B] Forwarding message from A to D, next hop D (port 5004)
[D] Received message from A: hello_from_A_to_D
```

### Quitting a node

```
quit
```

---

## 7. Algorithm Details

### TCP Tahoe

| Phase | Condition | cwnd Update |
|-------|-----------|-------------|
| **Slow Start** | `cwnd < ssthresh` | `cwnd × 2` per RTT (exponential) |
| **Congestion Avoidance** | `cwnd ≥ ssthresh` | `cwnd + 1 MSS` per RTT (linear) |
| **Timeout** | Loss detected by RTO | `ssthresh = cwnd/2`, `cwnd = 1 MSS` → Slow Start |
| **3 Dup ACK** | Three identical ACKs | **Same as Timeout in Tahoe** — `ssthresh = cwnd/2`, `cwnd = 1 MSS` |

> **Key property:** Tahoe has **no Fast Recovery**. Every loss event (regardless of type) resets `cwnd` to 1 MSS and restarts Slow Start. This is the main difference from TCP Reno.

Initial values:
- `cwnd = 1 MSS`
- `ssthresh = 64 MSS`
- Simulation length: 50 RTTs

### Routing — Dijkstra

Each node loads the **global topology** (hard-coded link weights) and runs Dijkstra from its own index to build a full routing table. When a message arrives, the node looks up the destination in the table, finds the next hop, and forwards the UDP packet accordingly.

---

## 8. Network Topology

```
        A (5001)
       /|\
      4 7 5\13
     /  |   \  \
    B   C    F  D
  (5002)(5003)(5006)(5004)
    |\ /|
    3  9
    E  |
  (5005)
    \
     12 (C-E)
```

### Link costs

| Link | Cost |
|------|------|
| A – B | 4 |
| A – C | 7 |
| A – D | 13 |
| A – F | 5 |
| B – D | 8 |
| B – E | 3 |
| C – D | 9 |
| C – E | 12 |

### Node A routing table (Dijkstra result)

| Destination | Next Hop | Total Cost | Path |
|-------------|----------|-----------|------|
| A | — | 0 | A |
| B | B | 4 | A → B |
| C | C | 7 | A → C |
| D | **B** | **12** | A → **B** → D *(direct A→D costs 13)* |
| E | B | 7 | A → B → E |
| F | F | 5 | A → F |

---

## 9. Configuration File Format

Each `.conf` file describes one node:

```
node_id   <single letter, e.g. A>
port      <UDP listen port>
neighbor  <neighbor_id> <neighbor_port> <link_cost>
neighbor  ...
```

**Example — `A.conf`:**

```
node_id A
port    5001
neighbor B 5002 4
neighbor C 5003 7
neighbor D 5004 13
neighbor F 5006 5
```

---

## 10. Sample Output

### TCP Tahoe Simulator

```
=================================================
        TCP Tahoe -- Congestion Control Sim
=================================================
RTT    | cwnd     | ssthresh   | Event
-------------------------------------------------
RTT  0 | cwnd=   1.0 | ssthresh=  64.0 | Initial (cwnd=1 MSS)
RTT  1 | cwnd=   2.0 | ssthresh=  64.0 | Slow Start
RTT  2 | cwnd=   4.0 | ssthresh=  64.0 | Slow Start
RTT  3 | cwnd=   8.0 | ssthresh=  64.0 | Slow Start
RTT  4 | cwnd=  16.0 | ssthresh=  64.0 | Slow Start
RTT  5 | cwnd=  32.0 | ssthresh=  64.0 | Slow Start
RTT  6 | cwnd=  64.0 | ssthresh=  64.0 | Slow Start
RTT  7 | cwnd=  65.0 | ssthresh=  64.0 | Cong. Avoid.
RTT  8 | cwnd=   1.0 | ssthresh=  32.5 | TIMEOUT -> ssthresh=cwnd/2, cwnd=1, Slow Start
RTT  9 | cwnd=   2.0 | ssthresh=  32.5 | Slow Start
...
RTT 18 | cwnd=   1.0 | ssthresh=  ...  | 3 DUP ACK -> ssthresh=cwnd/2, cwnd=1, Slow Start (Tahoe)
...
RTT 35 | cwnd=   1.0 | ssthresh=  ...  | TIMEOUT -> ssthresh=cwnd/2, cwnd=1, Slow Start
=================================================
```

### Routing Node Startup

```
[A] Listening on port 5001. Type 'send <DST> <msg>' or 'quit'.

[A] Routing Table (Dijkstra)
Destination  Next Hop   Cost
-----------------------------
A            A          0
B            B          4
C            C          7
D            B          12
E            B          7
F            F          5
```

---

*CSE 320 — Computer Networks Lab 5*
