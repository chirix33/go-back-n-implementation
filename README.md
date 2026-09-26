CS 538 - Additional Project, by Ashraf Abdul-Muumin

I decided to follow the development suggestion of 

# Reliable Data Transfer: Alternating-Bit Protocol and Go-Back-N

Two unidirectional (A -> B) reliable data transfer protocols implemented on
top of the provided network emulator (event-driven simulator with packet
loss, corruption, and delay).

## Status

- **Alternating-Bit Protocol (rdt3.0): implemented and tested.**
- Go-Back-N: not started yet.
- Report: not started yet (will cover both protocols once Go-Back-N is
  done).

## Contents

- `src/ABP_AbdulMuumin.c` - Alternating-Bit Protocol (rdt3.0), stop-and-wait
  with ACK and NACK.
- `Makefile` - builds `./abp`.
- `sample_outputs/` - captured trace-2 run(s) demonstrating recovery from
  loss and corruption.

## Building

```
make            # builds ./abp
make clean      # removes built binaries
```

Or compile directly:

```
gcc -o abp src/ABP_AbdulMuumin.c
```

No special flags are required; the portability fix described below is
already in the source.

## Running

The program prompts for:

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
