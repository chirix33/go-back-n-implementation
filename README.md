CS 538 - Additional Project, by Ashraf Abdul-Muumin

I decided to follow the development suggestion of the project, from the Professor. I made sure to incldue visible headings and sub-headings for easy navigation (forgive me if there's still clutered and verbose information)  

# Reliable Data Transfer: Alternating-Bit Protocol and Go-Back-N

Two unidirectional (A -> B) reliable data transfer protocols implemented on
top of the provided network emulator (event-driven simulator with packet
loss, corruption, and delay).

## Status

- **Alternating-Bit Protocol (rdt3.0): implemented and tested.**
- **Go-Back-N: in progress.** Reliable-channel operation, corruption
  handling, loss handling, and the mandated combined case (loss 0.2,
  corrupt 0.2) are implemented and tested; the two extra parameter
  combinations are still to do.
- Report: not started yet (will cover both protocols once Go-Back-N is
  done).

## Contents

- `src/ABP_AbdulMuumin.c` - Alternating-Bit Protocol (rdt3.0), stop-and-wait
  with ACK and NACK.
- `src/GBN_AbdulMuumin.c` - Go-Back-N (window N=8), in progress (see
  Status above for what's tested so far).
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
  (no modulo wraparound needed since the simulator always runs a bounded
  number of messages). Sender buffers up to 50 outstanding messages and
  sends as many as the window currently allows; it aborts gracefully
  (prints a message and exits) rather than crashing if that buffer would
  ever be exceeded.
- **Checksum**: covers `seqnum + acknum` plus the payload, computed
  identically on both sides, so a corrupted header field is caught the
  same way a corrupted payload is.
- **ACKs**: B sends a cumulative ACK for the highest in-order sequence
  number it has received. A corrupted or out-of-order packet at B is
  discarded and B re-ACKs its last correctly-received in-order packet
  (the GBN equivalent of a NACK — it is not relying on A's timeout alone
  to detect the problem).
- **Timer**: a single timer per the emulator's single-timer model, kept
  for the oldest unacked (base) packet; restarted whenever base advances
  but packets remain outstanding, stopped when the window is fully
  acked.
- Tested so far: (a) a fully reliable channel (loss = corrupt = 0,
  `sample_outputs/GBN_week2_reliable_loss0.0_corrupt0.0.log`) — all
  messages sent are delivered and cumulative-ACKed in order; (b)
  corruption-only (loss = 0, corrupt = 0.3,
  `sample_outputs/GBN_week2_corruptonly_loss0.0_corrupt0.3.log`) — the
  logs show both corrupted-payload and corrupted-header cases being
  caught by the checksum, B re-ACKing the last good packet, and A both
  discarding corrupted ACKs and recovering outstanding packets via
  timeout retransmission of the whole unacked window.

### Loss handling (loss-only test)

Run: 50 messages, loss = 0.2, corrupt = 0.0, mean arrival = 10, trace = 2
(`sample_outputs/GBN_week3_lossonly_loss0.2_corrupt0.0.log`). Result: 23
messages delivered and cumulatively ACKed, 38 packets lost in the channel,
10 timeouts, no buffer overflow.

Loss needs no extra code beyond the timer: a lost data packet makes B see a
gap and re-ACK the last good packet, and a lost ACK is covered by the next
cumulative ACK. If nothing gets through, A's timer fires and resends the
whole outstanding window. Example from the log (time 75-88): ACKs for seq 5
and 6 were both lost, so A's base stayed at 5. The timer fired and resent
5-7. B had already delivered 5 and 6, so it discarded the copies and
re-ACKed 6. The single cumulative ACK 6 then acknowledged both 5 and 6 at
once and base moved to 7.

### Mandated combined case

Run: 50 messages, loss = 0.2, corrupt = 0.2, mean arrival = 10, trace = 2
(`sample_outputs/GBN_week3_combined_loss0.2_corrupt0.2.log`). Result: 23
messages delivered and cumulatively ACKed (the requirement is at least
20), 50 packets lost, 29 corrupted, 12 timeouts, no buffer overflow. The
simulator stops once the last message has been handed to A, so a few
messages are normally still in flight at the end.

Note: the message count matters. With the same loss/corrupt settings and
100 messages, A's 50-slot buffer fills and the program aborts with its
"PANIC" message, because with these error rates GBN's throughput is below
one message per 10 time units. This is the specified abort behaviour, not a
crash.

### Timer tuning (A_TIMEOUT)

`A_TIMEOUT` is 25.0. One-way delay averages about 5 and queueing behind
other packets adds to that, so the round trip is well above 10. I swept
the timeout on the mandated case (50 messages, loss 0.2, corrupt 0.2, mean
arrival 10) over 20 different random seeds (the emulator normally uses a
fixed seed, which makes a single run noisy). Totals over the 20 runs:

| A_TIMEOUT | messages ACKed (total) | timeouts (total) |
|-----------|------------------------|------------------|
| 10        | 343                    | 846              |
| 15        | 412                    | 450              |
| 20        | 388                    | 322              |
| 25        | 403                    | 259              |
| 30        | 369                    | 196              |

A timeout of 10 is too short: the timer often fires while the ACK is still
on its way, causing many needless retransmissions and lower throughput.
Above 30, A sits idle too long after a real loss. Values around 15-25
perform about the same within the noise of this test; 25 was kept because
it sends almost half as many retransmissions as 15 for similar throughput.
It is a reasonable choice, not a proven optimum.

### Status after this phase

- Implemented and tested: reliable channel, corruption-only, loss-only, and
  the mandated combined case.
- Not done yet: the two additional required parameter combinations and the
  report.
