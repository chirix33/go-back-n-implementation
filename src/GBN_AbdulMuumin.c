#include <stdio.h>
#include <stdlib.h>

/* ******************************************************************
 GO-BACK-N PROTOCOL - unidirectional A -> B, window size N = 8
 NETWORK EMULATOR: VERSION 1.1  J.F.Kurose (base emulator, unmodified logic)

   Student protocol code (A_output, A_input, A_timerinterrupt, A_init,
   B_input, B_init) written by: Ashraf Abdul-Muumin
   CS 538, Project 2 - Part 2 (Go-Back-N)

   This code should be used for PA2, unidirectional data transfer
   protocols (from A to B. Bidirectional transfer of data is for extra
   credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
                           /* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
  };

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
    };

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/* ---------------------------------------------------------------------
   Design notes (Go-Back-N, window size N = 8):

   Sequence numbers are plain, monotonically increasing integers starting
   at 1 (no modulo wraparound). This is safe here because the simulator
   always stops after a bounded, user-chosen number of messages (nsimmax),
   so seqnum never approaches overflow; it avoids extra modular-arithmetic
   bookkeeping that the assignment does not require.

   Packet fields:
     - DATA packet: seqnum = message's sequence number, acknum = unused (0),
                     payload = message data.
     - ACK packet:  seqnum = 0 (unused), acknum = highest in-order sequence
                     number B has received so far (cumulative ACK, per GBN).

   checksum = seqnum + acknum + (sum of payload bytes as unsigned char),
   computed the same way on both sides, covering header fields as well as
   data (so a corrupted seq/ack field is also caught).

   Sender (A) buffering: up to MAXBUF (50) outstanding messages are held in
   a circular buffer, indexed by (seqnum-1) % MAXBUF. "base" is the oldest
   unacked sequence number; "nextseqnum" is the next sequence number to be
   handed to a new message from layer5; "sent_upto" is the highest sequence
   number actually transmitted so far. A_output() always assigns a sequence
   number and buffers the packet; try_send_window() then transmits as many
   buffered-but-unsent packets as the window (N=8) currently allows. This
   satisfies the assignment's requirement that A_output() buffer messages
   that arrive while the window is full, rather than dropping them, and
   send them once the window slides forward on a later ACK. If all MAXBUF
   buffer slots would be exceeded, the sender aborts (per the assignment's
   explicit instruction) since with the parameters used here this should
   never actually happen.

   Only a single timer is used (as provided by the emulator), for the
   oldest outstanding (base) packet, exactly as described in the GBN
   sender FSM: stop the timer when everything is acked, otherwise
   (re)start it whenever base advances but unacked packets remain.

   Receiver (B) does no buffering (matches the GBN receiver FSM): it
   accepts only the exact expected in-order packet, delivers it and ACKs
   it, and otherwise (out-of-order or corrupted) discards it and
   re-sends an ACK for the last correctly-received in-order packet.
--------------------------------------------------------------------- */

#define WINDOW_N      8
#define MAXBUF        50
/* Tuned empirically against the assignment's mandated test case (loss=0.2,
   corrupt=0.2, mean arrival=10, window=8): a timer this size keeps the
   sender retransmitting often enough to drain the window under heavy
   loss/corruption without triggering so many spurious timeouts (from
   queueing delay alone) that it makes congestion worse. See report for
   the buffer-overflow / stability discussion at very high loss + long runs. */
#define A_TIMEOUT     25.0

/* ---------------- A (sender) state ---------------- */
static struct pkt A_buffer[MAXBUF];  /* buffered/sent-but-unacked packets */
static int A_base;         /* oldest unacked sequence number */
static int A_nextseqnum;   /* next sequence number to assign to new data */
static int A_sent_upto;    /* highest sequence number actually sent so far */

static int compute_checksum(seqnum, acknum, payload)
int seqnum, acknum;
char payload[20];
{
  int sum, i;
  sum = seqnum + acknum;
  for (i = 0; i < 20; i++)
    sum += (unsigned char) payload[i];
  return sum;
}

static int is_corrupt(packet)
struct pkt packet;
{
  return (packet.checksum != compute_checksum(packet.seqnum, packet.acknum, packet.payload));
}

/* send as many buffered-but-not-yet-sent packets as the window allows */
static void try_send_window()
{
  int idx;

  while (A_sent_upto < A_nextseqnum - 1 && (A_sent_upto + 1 - A_base) < WINDOW_N) {
    A_sent_upto++;
    idx = (A_sent_upto - 1) % MAXBUF;
    printf("A: window has room, sending buffered seq %d (base=%d, sent_upto=%d)\n",
           A_sent_upto, A_base, A_sent_upto);
    tolayer3(0, A_buffer[idx]);
    if (A_base == A_sent_upto)
      starttimer(0, A_TIMEOUT);
    }
}

/* called from layer 5, passed the data to be sent to other side */
A_output(message)
  struct msg message;
{
  int seq, idx, i;

  if (A_nextseqnum - A_base >= MAXBUF) {
    printf("A_output: PANIC - all %d sender buffers in use, aborting\n", MAXBUF);
    exit(1);
    }

  seq = A_nextseqnum;
  A_nextseqnum++;
  idx = (seq - 1) % MAXBUF;

  A_buffer[idx].seqnum = seq;
  A_buffer[idx].acknum = 0;
  for (i = 0; i < 20; i++)
    A_buffer[idx].payload[i] = message.data[i];
  A_buffer[idx].checksum = compute_checksum(seq, 0, A_buffer[idx].payload);

  printf("A_output: new message assigned seq %d\n", seq);
  try_send_window();
}

B_output(message)  /* need be completed only for extra credit */
  struct msg message;
{

}

/* called from layer 3, when a packet arrives for layer 4 */
A_input(packet)
  struct pkt packet;
{
  if (is_corrupt(packet)) {
    printf("A_input: corrupted ack packet received, ignoring (will rely on timeout)\n");
    return;
    }

  if (packet.acknum < A_base) {
    printf("A_input: old/duplicate ACK %d received (base already %d), ignoring\n",
           packet.acknum, A_base);
    return;
    }

  printf("A_input: valid cumulative ACK %d received\n", packet.acknum);
  A_base = packet.acknum + 1;

  if (A_base > A_sent_upto) {
    printf("A_input: all outstanding packets acked, stopping timer\n");
    stoptimer(0);
    }
  else {
    printf("A_input: %d packet(s) still outstanding, restarting timer\n", A_sent_upto - A_base + 1);
    stoptimer(0);
    starttimer(0, A_TIMEOUT);
    }

  try_send_window();
}

/* called when A's timer goes off */
A_timerinterrupt()
{
  int s, idx;

  printf("A_timerinterrupt: timeout! retransmitting seq %d through %d\n", A_base, A_sent_upto);
  for (s = A_base; s <= A_sent_upto; s++) {
    idx = (s - 1) % MAXBUF;
    tolayer3(0, A_buffer[idx]);
    }
  starttimer(0, A_TIMEOUT);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
A_init()
{
  A_base = 1;
  A_nextseqnum = 1;
  A_sent_upto = 0;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* ---------------- B (receiver) state ---------------- */
static int B_expectedseqnum;  /* next in-order sequence number B expects */

/* called from layer 3, when a packet arrives for layer 4 at B*/
B_input(packet)
  struct pkt packet;
{
  struct pkt ack;

  if (!is_corrupt(packet) && packet.seqnum == B_expectedseqnum) {
    printf("B_input: correct in-order packet, seq %d, delivering to layer5\n", packet.seqnum);
    tolayer5(1, packet.payload);
    ack.acknum = B_expectedseqnum;
    B_expectedseqnum++;
    }
  else {
    if (is_corrupt(packet))
      printf("B_input: corrupted packet received, re-acking %d\n", B_expectedseqnum - 1);
    else
      printf("B_input: out-of-order packet (seq %d, expected %d), discarding, re-acking %d\n",
             packet.seqnum, B_expectedseqnum, B_expectedseqnum - 1);
    ack.acknum = B_expectedseqnum - 1;
    }

  ack.seqnum = 0;
  ack.checksum = compute_checksum(ack.seqnum, ack.acknum, ack.payload);
  tolayer3(1, ack);
}

/* called when B's timer goes off */
B_timerinterrupt()
{
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
B_init()
{
  B_expectedseqnum = 1;
}


/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify

  Portability note: the only change made to the emulator below (vs. the
  distributed prog2.c) is including <stdlib.h>, passing an explicit
  status code to exit(), and using RAND_MAX instead of a hardcoded
  2147483647 in jimsrand() (this machine's rand() only returns values up
  to RAND_MAX, so the hardcoded constant made the emulator's own
  random-number self-test fail). No emulator behavior was changed.
******************************************************************/

struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

main()
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;

   int i,j;
   char c;

   init();
   A_init();
   B_init();

   while (1) {
        eventptr = evlist;            /* get next event to simulate */
        if (eventptr==NULL)
           goto terminate;
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist!=NULL)
           evlist->prev=NULL;
        if (TRACE>=2) {
           printf("\nEVENT time: %f,",eventptr->evtime);
           printf("  type: %d",eventptr->evtype);
           if (eventptr->evtype==0)
	       printf(", timerinterrupt  ");
             else if (eventptr->evtype==1)
               printf(", fromlayer5 ");
             else
	     printf(", fromlayer3 ");
           printf(" entity: %d\n",eventptr->eventity);
           }
        time = eventptr->evtime;        /* update time to next event time */
        if (nsim==nsimmax)
	  break;                        /* all done with simulation */
        if (eventptr->evtype == FROM_LAYER5 ) {
            generate_next_arrival();   /* set up future arrival */
            /* fill in msg to give with string of same letter */
            j = nsim % 26;
            for (i=0; i<20; i++)
               msg2give.data[i] = 97 + j;
            if (TRACE>2) {
               printf("          MAINLOOP: data given to student: ");
                 for (i=0; i<20; i++)
                  printf("%c", msg2give.data[i]);
               printf("\n");
	     }
            nsim++;
            if (eventptr->eventity == A)
               A_output(msg2give);
             else
               B_output(msg2give);
            }
          else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i=0; i<20; i++)
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
	    if (eventptr->eventity ==A)      /* deliver packet by calling */
   	       A_input(pkt2give);            /* appropriate entity */
            else
   	       B_input(pkt2give);
	    free(eventptr->pktptr);          /* free the memory for packet */
            }
          else if (eventptr->evtype ==  TIMER_INTERRUPT) {
            if (eventptr->eventity == A)
	       A_timerinterrupt();
             else
	       B_timerinterrupt();
             }
          else  {
	     printf("INTERNAL PANIC: unknown event type \n");
             }
        free(eventptr);
        }

terminate:
   printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time,nsim);
}



init()                         /* initialize the simulator */
{
  int i;
  float sum, avg;
  float jimsrand();


   printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
   printf("Enter the number of messages to simulate: ");
   scanf("%d",&nsimmax);
   printf("Enter  packet loss probability [enter 0.0 for no loss]:");
   scanf("%f",&lossprob);
   printf("Enter packet corruption probability [0.0 for no corruption]:");
   scanf("%f",&corruptprob);
   printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
   scanf("%f",&lambda);
   printf("Enter TRACE:");
   scanf("%d",&TRACE);

   srand(9999);              /* init random number generator */
   sum = 0.0;                /* test random number generator for students */
   for (i=0; i<1000; i++)
      sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
   avg = sum/1000.0;
   if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n" );
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(1);
    }

   ntolayer3 = 0;
   nlost = 0;
   ncorrupt = 0;

   time=0.0;                    /* initialize time to 0.0 */
   generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
  /* Portability fix: rand() on this platform only returns up to RAND_MAX
     (e.g. 32767 on MinGW/Windows), not INT_MAX, so the emulator's own
     self-test (avg of jimsrand() over 1000 draws) would otherwise fail.
     Using RAND_MAX keeps jimsrand() uniform on [0,1] as required. */
  double mmm = RAND_MAX;      /* largest value rand() can return here */
  float x;                   /* individual students may need to change mmm */
  x = rand()/mmm;            /* x should be uniform in [0,1] */
  return(x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

generate_next_arrival()
{
   double x,log(),ceil();
   struct event *evptr;
   float ttime;
   int tempint;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

   x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + x;
   evptr->evtype =  FROM_LAYER5;
   if (BIDIRECTIONAL && (jimsrand()>0.5) )
      evptr->eventity = B;
    else
      evptr->eventity = A;
   insertevent(evptr);
}


insertevent(p)
   struct event *p;
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",time);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime);
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q;
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}

printevlist()
{
  struct event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
 struct event *q,*qold;

 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time);
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
 for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
       /* remove this event */
       if (q->next==NULL && q->prev==NULL)
             evlist=NULL;         /* remove first and only event on list */
          else if (q->next==NULL) /* end of list - there is one in front */
             q->prev->next = NULL;
          else if (q==evlist) { /* front of list - there must be event after */
             q->next->prev=NULL;
             evlist = q->next;
             }
           else {     /* middle of list */
             q->next->prev = q->prev;
             q->prev->next =  q->next;
             }
       free(q);
       return;
     }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{

 struct event *q;
 struct event *evptr;

 if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",time);
 /* be nice: check to see if timer is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
   for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }

/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
}


/************************** TOLAYER3 ***************/
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 float lastime, x, jimsrand();
 int i;


 ntolayer3++;

 /* simulate losses: */
 if (jimsrand() < lossprob)  {
      nlost++;
      if (TRACE>0)
	printf("          TOLAYER3: packet being lost\n");
      return;
    }

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */
 mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
 mypktptr->seqnum = packet.seqnum;
 mypktptr->acknum = packet.acknum;
 mypktptr->checksum = packet.checksum;
 for (i=0; i<20; i++)
    mypktptr->payload[i] = packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
	  mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<20; i++)
        printf("%c",mypktptr->payload[i]);
    printf("\n");
   }

/* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
 lastime = time;
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
 for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) )
      lastime = q->evtime;
 evptr->evtime =  lastime + 1 + 9*jimsrand();



 /* simulate corruption: */
 if (jimsrand() < corruptprob)  {
    ncorrupt++;
    if ( (x = jimsrand()) < .75)
       mypktptr->payload[0]='Z';   /* corrupt payload */
      else if (x < .875)
       mypktptr->seqnum = 999999;
      else
       mypktptr->acknum = 999999;
    if (TRACE>0)
	printf("          TOLAYER3: packet being corrupted\n");
    }

  if (TRACE>2)
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
}

tolayer5(AorB,datasent)
  int AorB;
  char datasent[20];
{
  int i;
  if (TRACE>2) {
     printf("          TOLAYER5: data received: ");
     for (i=0; i<20; i++)
        printf("%c",datasent[i]);
     printf("\n");
   }

}
