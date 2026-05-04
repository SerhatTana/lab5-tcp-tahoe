# CSE 320 Lab 5: TCP Tahoe Congestion Control and UDP Routing

This project demonstrates a console-based network application written in C. Six nodes communicate over UDP sockets, calculate shortest paths with Dijkstra, and simulate TCP Tahoe congestion-control behavior when a message is sent.

Students:
- 20220808001 Serhat Bugra Tana
- 20220808006 Alperen Ulukaya

Formula:

```text
(20220808001 + 20220808006) mod 3 = 0 -> TCP Tahoe
```

Only TCP Tahoe is implemented. There is no TCP Reno, NewReno, or graphical interface.

## Files

| File | Purpose |
| ---- | ------- |
| `node.c` | Main UDP routing program and TCP Tahoe simulation |
| `node.h` | Shared constants and data structures |
| `tcp_sim.c` | Standalone TCP Tahoe simulator |
| `A.conf` ... `F.conf` | Node configuration files |
| `Makefile` | Build rules |

## Build

```bash
make
```

Manual build on Windows:

```bash
gcc -Wall -Wextra -o node node.c -lws2_32
gcc -Wall -Wextra -o tcp_sim tcp_sim.c
```

Manual build on Linux/macOS:

```bash
gcc -Wall -Wextra -o node node.c
gcc -Wall -Wextra -o tcp_sim tcp_sim.c
```

## Run the Six Nodes

Open six terminals:

```bash
./node A.conf
./node B.conf
./node C.conf
./node D.conf
./node E.conf
./node F.conf
```

In Windows PowerShell:

```powershell
.\node.exe A.conf
.\node.exe B.conf
.\node.exe C.conf
.\node.exe D.conf
.\node.exe E.conf
.\node.exe F.conf
```

## Send a Message

From Node A:

```text
send D hello_from_A_to_D
```

Expected route:

```text
[A] Destination D, next hop B (cost 12)
[B] Forwarding message from A to D, next hop D
[D] Received message from A: hello_from_A_to_D
```

Dijkstra selects `A -> B -> D` because its total cost is 12, which is lower than the direct `A -> D` cost of 13.

## TCP Tahoe Behavior

When a `send` command is entered, the sender first runs a TCP Tahoe simulation and prints `cwnd` step by step.

Implemented Tahoe rules:
- Slow Start: while `cwnd < ssthresh`, `cwnd` doubles each RTT.
- Congestion Avoidance: after reaching `ssthresh`, `cwnd` increases linearly by `+1 MSS`.
- Timeout or packet loss: `ssthresh = cwnd / 2`, then `cwnd = 1 MSS`.
- Triple duplicate ACK / Fast Retransmit: Tahoe also resets `cwnd` to `1 MSS`.

The packet-loss pattern is configurable in `node.c` through the `TAHOE_EVENTS` table.

## Sample Tahoe Output

```text
[A] TCP Tahoe simulation before sending to D
[TCP Tahoe] RTT  0 | cwnd=  1.0 MSS | ssthresh= 16.0 | Initial sender state
[TCP Tahoe] RTT  1 | cwnd=  2.0 MSS | ssthresh= 16.0 | Slow Start
[TCP Tahoe] RTT  2 | cwnd=  4.0 MSS | ssthresh= 16.0 | Slow Start
[TCP Tahoe] RTT  3 | cwnd=  8.0 MSS | ssthresh= 16.0 | Slow Start
[TCP Tahoe] RTT  4 | cwnd=  1.0 MSS | ssthresh=  4.0 | TIMEOUT / packet loss -> ssthresh=cwnd/2, cwnd=1
```

This output shows the required Tahoe behavior: after packet loss, `cwnd` immediately resets to `1 MSS`.

## Standalone Tahoe Simulator

The separate simulator can also be run:

```bash
./tcp_sim tahoe
```

Windows PowerShell:

```powershell
.\tcp_sim.exe tahoe
```

## Demo Checklist

The demonstration should show:
- six terminals running nodes A through F
- A routing table where D uses next hop B with cost 12
- `send D hello_from_A_to_D` from Node A
- hop-by-hop forwarding logs
- TCP Tahoe `cwnd` evolution before message transmission
- timeout or packet loss causing `cwnd` to reset to `1 MSS`
