CS 538 - Additional Project, by Ashraf Abdul-Muumin

I decided to follow the development suggestion of the project, from the Professor. I made sure to incldue visible headings and sub-headings for easy navigation (forgive me if there's still clutered and verbose information)  

# Reliable Data Transfer: Alternating-Bit Protocol and Go-Back-N

Two unidirectional (A -> B) reliable data transfer protocols implemented on
top of the provided network emulator (event-driven simulator with packet
loss, corruption, and delay).

## Status

- **Alternating-Bit Protocol (rdt3.0): implemented and tested.**
- **Go-Back-N (window 8): implemented and tested** - reliable channel,
  corruption only, loss only, the mandated combined case and two more
  parameter combinations.
- **Report:** `report/report.tex` / `report/report.pdf` (IEEE, two column,
  2 pages).

## Contents

- `src/ABP_AbdulMuumin.c` - Alternating-Bit Protocol (rdt3.0), stop-and-wait
  with ACK and NACK.
- `src/GBN_AbdulMuumin.c` - Go-Back-N (window N=8), with ACK and NACK.
- `report/` - the report (`report.tex`, `report.pdf`).
- `Makefile` - builds `./abp` and `./gbn`.
- `sample_outputs/` - captured trace-2 run(s) demonstrating recovery from
  loss and corruption.

## Building

```
make            # builds ./abp and ./gbn
make clean      # removes built binaries
```

Or compile directly:

```
gcc -o abp src/ABP_AbdulMuumin.c
gcc -o gbn src/GBN_AbdulMuumin.c
```

No special flags are required; the portability fix described below is
already in the source (both files share the same emulator base).

## Running

Both programs prompt for:

1. Number of messages to simulate (must be > 1)
2. Packet loss probability (0.0 for none)
3. Packet corruption probability (0.0 for none)
4. Average time between messages from the sender's layer 5 (> 0.0)
5. Trace level (0 = silent, 2 = recommended for debugging/inspection)

## Portability fixes to the emulator

The emulator half of `prog2.c` (the code students are told not to modify)
needed two small, behavior-preserving changes to build and run correctly on
a modern toolchain:

1. `#include <stdlib.h>` was added and `exit()` calls now pass an explicit
   status code (`exit(1)`), since modern compilers require this and no
   longer implicitly declare `exit`/`malloc`/`free`.
2. `jimsrand()` originally divided by a hardcoded `mmm = 2147483647`
   (`INT_MAX`), assuming `rand()` can return values up to `INT_MAX`. On
   this build's C library `RAND_MAX` is smaller, which made the emulator's
   own random-number self-test fail immediately on startup. `mmm` now uses
   `RAND_MAX` instead, which keeps `jimsrand()` uniform on `[0,1]` as
   required and is itself portable to any platform's `rand()` range - this
   is exactly the kind of machine-dependent adjustment the assignment
   description anticipates in its "Random Numbers" hint.

No other emulator code, and no data structure, was changed.

## Design summary (Alternating-Bit Protocol)

- **Checksum**: sum of `seqnum + acknum` plus a byte-wise sum of the 20
  payload bytes, computed identically by both sides.
- One outstanding packet at a time; B replies with ACK(seq) on a correct
  in-order packet, or a NACK (sentinel `acknum = -1`) on a
  corrupted/unexpected packet, triggering an immediate retransmission at A
  (rather than waiting for the timer) in addition to the normal timeout
  path.

## Design summary (Go-Back-N, window N=8)

- **Window**: sliding window of size 8, plain increasing sequence numbers
  (no wraparound; the simulator runs a bounded number of messages). The
  sender buffers up to 50 outstanding messages and sends as many as the
  window allows; if the buffer would be exceeded it prints a message and
  exits.
- **Checksum**: `seqnum + acknum` plus the payload bytes, computed
  identically on both sides, so a corrupted header field is caught like a
  corrupted payload.
- **ACK**: B sends a cumulative ACK (`seqnum = 0`, `acknum` = highest
  in-order packet delivered).
- **NACK**: when B gets a corrupted packet, or one that skips ahead (a
  gap), it discards it and sends a NACK (`seqnum = 1`, `acknum` = last
  in-order packet delivered). A stale duplicate gets a plain re-ACK. On a
  valid NACK that says B still wants `base`, A resends the window at once.
  A reacts to only one NACK per `base` value (otherwise every packet
  behind a lost one would trigger another full resend); after that the
  timer takes over.
- **Timer**: one timer, for the oldest unacked packet; restarted when
  `base` advances and packets remain, stopped when everything is acked. On
  timeout the whole unacked range is resent.

## Test runs (trace level 2)

All logs in `sample_outputs/` were produced by the final code. "Confirmed"
is the highest cumulative ACK that reached A. The run column gives
messages / loss / corrupt / mean arrival.

| Log | Run | Confirmed |
|-----|-----|-----------|
| `ABP_loss0.1_corrupt0.3.log` | ABP: 12 / 0.1 / 0.3 / 1000 | 10 |
| `GBN_reliable_loss0.0_corrupt0.0.log` | GBN: 20 / 0 / 0 / 10 | 19 |
| `GBN_corruptonly_loss0.0_corrupt0.3.log` | GBN: 20 / 0 / 0.3 / 10 | 10 |
| `GBN_lossonly_loss0.2_corrupt0.0.log` | GBN: 50 / 0.2 / 0 / 10 | 46 |
| `GBN_case1_loss0.2_corrupt0.2_lambda10.log` | GBN mandated: 50 / 0.2 / 0.2 / 10 | 22 |
| `GBN_case2_loss0.35_corrupt0.1_lambda12.log` | GBN extra: 60 / 0.35 / 0.1 / 12 | 25 |
| `GBN_case3_loss0.1_corrupt0.15_lambda8.log` | GBN extra: 60 / 0.1 / 0.15 / 8 | 27 |

The simulator stops once the last message is handed to A, so a few
messages are normally still in flight at the end. None of these runs hit
the 50-buffer abort. The message count matters: with loss 0.2 and corrupt
0.2, 100 messages fills the 50-slot buffer and the program aborts with its
"PANIC" message (the specified behaviour, not a crash); use about 50.

To re-run one case, e.g. the mandated one:

```
printf "50
0.2
0.2
10
2
" | ./gbn
```

### Timer tuning (A_TIMEOUT)

`A_TIMEOUT` is 25.0. One-way delay averages about 5 and queueing adds to
it, so the round trip is well above 10. I swept the timeout on the mandated
case (50 messages, loss 0.2, corrupt 0.2, mean arrival 10) over 20 random
seeds (the emulator normally uses a fixed seed, which makes one run
noisy). Totals over the 20 runs:

| A_TIMEOUT | messages confirmed (total) | timeouts (total) |
|-----------|----------------------------|------------------|
| 10        | 350                        | 769              |
| 15        | 373                        | 426              |
| 20        | 424                        | 284              |
| 25        | 427                        | 193              |
| 30        | 421                        | 141              |

10-15 cause many spurious timeouts. From 20 up throughput is flat within
the noise while retransmissions keep falling; 25 was kept as a reasonable
choice, not a proven optimum.
