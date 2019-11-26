#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <string.h>

/* ******************************************************************
   ARQ NETWORK EMULATOR: VERSION 1.1  J.F.Kurose
   MODIFIED by Chong Wang on Oct.21,2005 for csa2,csa3 environments

   This code should be used for PA2, unidirectional data transfer protocols 
   (from A to B)
   Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for Pipelined ARQ), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

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


/*- Your Definitions 
  ---------------------------------------------------------------------------*/

/* Please use the following values in your program */

#define   A    0
#define   B    1
#define   FIRST_SEQNO   0

/*- Declarations ------------------------------------------------------------*/
void	restart_rxmt_timer(void);
void	tolayer3(int AorB, struct pkt packet);
void	tolayer5(char datasent[20]);

void	starttimer(int AorB, double increment);
void	stoptimer(int AorB);

/* WINDOW_SIZE, RXMT_TIMEOUT and TRACE are inputs to the program;
   Please set an appropriate value for LIMIT_SEQNO.
   You have to use these variables in your 
   routines --------------------------------------------------------------*/

extern int WINDOW_SIZE;      // size of the window
extern int LIMIT_SEQNO;      // when sequence number reaches this value, it wraps around
extern double RXMT_TIMEOUT;  // retransmission timeout
extern int TRACE;            // trace level, for your debug purpose
extern double time_now;      // simulation time, for your debug purpose

/********* YOU MAY ADD SOME ROUTINES HERE ********/

/* MY VARIABLES */

#define CAPACITY 50
#define INIT_WINDOW_SIZE 10
#define INIT_RTT 10

int BASE_SEQNUM;      // sequence number of oldest transmitted packet 
int NEXT_SEQNUM;      // sequence number of next packet to be transmitted
int SPACE;            // amount of available space in A-window
int QUEUE_SIZE;       // number of packets in buffer currently
int WINDOW_INDEX;     // number of packets in A-window currently
int B_PACKETNUM;      // next expected sequence number in B
int B_INDEX;          // index at which to add next package

/* WINDOWS AND BUFFERS */

struct pkt *(WINDOW[INIT_WINDOW_SIZE]);       // A-side window, holds transmitted packets
struct pkt *(B_WINDOW[INIT_WINDOW_SIZE]);     // B-side window, holds transmitted acks
struct msg *(BUFFER[CAPACITY]);               // A-side buffer, holds untransmitted packets

/* DATA */

int NET_TIME;                 // includes retransmissions
int ONESHOT_TIME;             // excludes retransmissions
int ORIGINAL_PACKETS;
int CORRUPTED_PACKETS;
int B_ACKS;
int PACKETS_DELIVERED;
int RETRANSMISSIONS;
// [start time][retransmitted]
double** rtt;//[INIT_RTT][2];

// CHECKSUM FUNCTIONS

int compute_checksum(int seq, int ack, char data[20]){
  int checksum = 0;
  checksum += seq;
  checksum += ack;
  for (int x = 0; x < 20; x++){
    checksum += data[x];
  }
  return checksum;
}

// SEQUENCE NUMBER FUNCTIONS

void inc_B_seq(){
  if (B_PACKETNUM >= LIMIT_SEQNO){
    B_PACKETNUM = 0;
  } else {
    B_PACKETNUM++;
  }
  return;
}

void inc_seq(){
  if (NEXT_SEQNUM >= LIMIT_SEQNO){
    NEXT_SEQNUM = 0;
  } else {
    NEXT_SEQNUM++;
    //SPACE--;
  }
  if (TRACE >= 3){
    printf("INC_NEXT: packet sent; next sequence number increased to %d\n", NEXT_SEQNUM);
  }
  return;
}

void inc_base(int s){
  stoptimer(A);

  // calculate total time

  /*
  double t = time_now - rtt[s][0];

  if (rtt[s][1] == 0.0){
    ONESHOT_TIME += t;
  } 
  
  NET_TIME += t;

  rtt[s][0] = 0.0;
  rtt[s][1] = 0.0;
  */

  if (BASE_SEQNUM >= LIMIT_SEQNO){
    BASE_SEQNUM = 0;
  } else {
    BASE_SEQNUM++;
    //SPACE++;
  }
  if (TRACE >= 3) {
    printf("INC_BASE: ack received; base sequence number increased to %d\n", BASE_SEQNUM);
  }
  return;
}

/********* WINDOW FUNCTIONS ***********/

// initialize window
void window_init(){
  for (int i = 0; i < WINDOW_SIZE; i++){
    WINDOW[i] = malloc(sizeof(struct pkt));
  }
}

// check window for packet with sequence number s, if it exists return index, else -1
int window_index(int s){
  for (int i = 0; i < WINDOW_SIZE; i++){
    if (WINDOW[i]->seqnum == s){
      return i;
    }
  }
  return -1;
}

void B_window_init(){
  for (int i = 0; i < WINDOW_SIZE; i++){
    B_WINDOW[i] = malloc(sizeof(struct pkt));
  }
}

// add new packet to window
void add_window(struct pkt *p){

  //set start time and indicate not retransmitted
  int s = p->seqnum;

  /*
  printf("seqnum is %d and rtt[s][0] is: %f\n", s, rtt[s][0]);

  rtt[s][0] = time_now;
  rtt[s][1] = 0;
  */

  if (WINDOW_INDEX >= WINDOW_SIZE){
    if (TRACE >= 1){
      printf("WINDOW FULL!\n");
    }
    return;
  }

  if (WINDOW_INDEX == 0){
    starttimer(A, RXMT_TIMEOUT);
  }
  WINDOW[WINDOW_INDEX] = p;
  WINDOW_INDEX++;
  SPACE--;
  return;
}

// slide the window over
void update_window(){
  for (int i = 1; i < WINDOW_INDEX; i++){
    WINDOW[i-1] = WINDOW[i];
  }
  WINDOW_INDEX--;
  SPACE++;
  starttimer(A, RXMT_TIMEOUT);
  return;
}

// print the window
void print_window(){
  printf("WINDOW:\n");
  printf("[");
  for (int i = 0; i < WINDOW_INDEX; i++){
    printf("PACKET %d, ", WINDOW[i]->seqnum);
  }
  printf("]\n");
  return;
}

// retransmit all packets in the window
void retransmit(){

  print_window();

  if (WINDOW_INDEX == 0){
    if (TRACE >= 1){
      printf("WINDOW EMPTY. NOTHING TO RETRANSMIT\n");
    }
    return;
  }

  struct pkt *p = malloc(sizeof(struct pkt));
  for (int x = 0; x < WINDOW_INDEX; x++){
    p = WINDOW[x];
    int s = p->seqnum;
    
    // if it hasn't been acknowleged already, indicate it's been retransmitted
    /*
    if (rtt[s][0] > 0.0){
      rtt[s][1] = 1;
    }
    */
    tolayer3(A, *p);
  }
  if (TRACE >= 1){
    printf("RETRANSMITTING: up to packet %d\n", p->seqnum);
  }
  RETRANSMISSIONS++;
  starttimer(A, RXMT_TIMEOUT);
  return;
}

/********* B_WINDOW FUNCTIONS **********/

// print the window
void B_print_window(){
  printf("B_WINDOW:\n");
  printf("[");
  for (int i = 0; i < B_INDEX; i++){
    printf("PACKET %d, ", B_WINDOW[i]->seqnum);
  }
  printf("]\n");
  return;
}

// add new packet to window
void B_add_window(struct pkt *p){

  // if the window is full
  if (B_INDEX >= WINDOW_SIZE){

    // slide the window to make room for next packet
    for (int i = 1; i < B_INDEX; i++){
      B_WINDOW[i-1] = B_WINDOW[i];
    } 
    B_WINDOW[WINDOW_SIZE-1] = p;
  } else {
    B_WINDOW[B_INDEX] = p;
    B_INDEX++;
  }
  return;
}

// check window for packet with ack number s, if it exists return index, else -1
int B_window_index(int a){
  for (int i = 0; i < WINDOW_SIZE; i++){
    if (B_WINDOW[i]->acknum == a){
      return i;
    }
  }
  return -1;
}

// send or resend an ack
void B_ack(struct pkt p){

  int r_checksum = p.checksum;
  int r_acknum = p.acknum;
  int r_seqnum = p.seqnum;
  char r_data[20];
  strncpy(r_data, p.payload, 20);

  // deliver up a layer
  tolayer5(r_data);
  PACKETS_DELIVERED++;

  // SEND ACK
  struct pkt ack;
  ack.seqnum = r_seqnum;
  ack.acknum = r_seqnum;
  ack.checksum = compute_checksum(ack.seqnum, ack.acknum, r_data);
	strncpy(ack.payload, r_data, 20);

  // send to A
  tolayer3(B, ack);
  B_ACKS++;      

  if (TRACE >= 1){
    printf("B: Sent ack %d\n", r_seqnum);
  }

  inc_B_seq();

  return;
}

/********* QUEUE FUNCTIONS ************/

// initialize buffer
void buffer_init(){
  for (int i = 0; i < CAPACITY; i++){
    BUFFER[i] = malloc(sizeof(struct msg));
  }
}

// enqueue message
void enqueue(struct msg m){
  if (QUEUE_SIZE >= CAPACITY){
    return;
  }
  strncpy(BUFFER[QUEUE_SIZE]->data, m.data, 20);
  QUEUE_SIZE++;
  return;
}

// dequeue message
struct msg dequeue(){
  struct msg *item = BUFFER[0];
  for (int i = 1; i < QUEUE_SIZE; i++){
    strncpy(BUFFER[QUEUE_SIZE]->data, BUFFER[i]->data, 20);
  }
  QUEUE_SIZE--;
  return *item;
}

// print queue, mostly for debugging purposes
void print_queue(){
  printf("BUFFER:\n");
  printf("[");
  for (int i = 0; i < QUEUE_SIZE; i++){
    printf("%s, ", BUFFER[i]->data);
  }
  printf("]\n");
  return;
}

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void
A_output (message)
    struct msg message;
{

  // ATTEMPT TO ADD MESSAGE TO QUEUE
  if (QUEUE_SIZE >= CAPACITY){
    printf("ABORT: QUEUE HAS OVERFLOWN\n");
    return;
  // QUEUE HAS SPACE; ADD MESSAGE TO END OF QUEUE
  } else {
    enqueue(message);
  } 

  if (TRACE >= 2){
    print_queue();
  }

  // IF WE HAVE SPACE IN THE WINDOW, SEND NEXT MESSAGE
  if (SPACE > 0){

    message = dequeue();

	  // CONSTRUCT PACKET
  	struct pkt *packet = malloc(sizeof(struct pkt));
	  packet->seqnum = NEXT_SEQNUM;
	  packet->acknum = 0;
	  packet->checksum = compute_checksum(NEXT_SEQNUM, 0, message.data);
	  strncpy(packet->payload, message.data, 20);
    
    // SEND PACKET
	  tolayer3(A, *packet);
    add_window(packet);

    if (TRACE >= 2){
      print_window();
    }

    // COLLECT DATA & ADJUST STATS
    ORIGINAL_PACKETS++;
    inc_seq();

    if (TRACE >= 1){
      printf("SENT MESSAGE: SEQ_NUM %d MESSAGE %s\n", packet->seqnum, message.data);
    }
  } else {

    if (TRACE >= 2){
      printf("WINDOW FULL\n");
      print_window();
    }

  }

  return;
}

/* called from layer 3, when a packet arrives for layer 4 */
void
A_input(packet)
  struct pkt packet;
{	

  int r_checksum = packet.checksum;
  int r_acknum = packet.acknum;
  int r_seqnum = packet.seqnum;
  char r_data[20];
  strncpy(r_data, packet.payload, 20);

  // PACKET RECEIVED BY B
  if (TRACE >= 1){
    printf("ACK %d RECEIVED BY A\n", r_acknum);
  }

  int computed_checksum = compute_checksum(r_seqnum, r_acknum, r_data);

  // check for corruption
  if (computed_checksum == r_checksum){

    int index = window_index(r_acknum);

    if (index > -1){//if (r_acknum >= BASE_SEQNUM && (index < WINDOW_SIZE - SPACE)){

      if (TRACE >= 2){
        printf("ack %d is in the window\n", r_acknum);
      }

      // if this packet has not been acknowledged yet, acknowledge
      if (WINDOW[index]->acknum == 0){

        WINDOW[index]->acknum == 1;

        if (TRACE >= 2){
          printf("A: packet %d has been acknowleged\n", r_acknum);
        }

      // duplicate ack, retransmit
      } else {

        if (TRACE >= 2){
          printf("DUPLICATE ACK: RETRANSMIT\n");
        }
        retransmit();
      }

      // SLIDE WINDOW

      for (int i = index; i > 0; i--){
        if (WINDOW[index]->acknum == 0){
          return;
        }
      }

      for (int x = 0; x < index + 1; x++){
        int s = WINDOW[index]->seqnum;
        inc_base(s);
        update_window();
      }

    // otherwise, ignore
    } else {
      return;
    }
  } else {
    if (TRACE >= 2){
      printf("ACK %d: received corrupted\n", r_acknum);
    }
  }
  return;
}

/* called when A's timer goes off */
void
A_timerinterrupt (void)
{

  if (TRACE >= 1){
    printf("TIMER INTERRUPTED!\n");
  }

  retransmit();
  
  return;
} 

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void
A_init (void)
{

  // SET FUNCTIONAL VARIABLES
  BASE_SEQNUM = 0;
  NEXT_SEQNUM = 0;
  SPACE = WINDOW_SIZE;
  QUEUE_SIZE = 0;
  WINDOW_INDEX = 0;
  NET_TIME = 0;
  ONESHOT_TIME = 0;
  
  // SET DATA COLLECTION VARIABLES
  ORIGINAL_PACKETS = 0;
  CORRUPTED_PACKETS = 0;
  PACKETS_DELIVERED = 0;
  B_ACKS = 0;

  // reset rtt to have size limit_seqno
 /* for (int i = 0; i < LIMIT_SEQNO; i++){
    rtt[i] = malloc(2* sizeof(double));
  }*/

  if (TRACE >= 2){
    printf("LIMIT_SEQNO is: %d\n", LIMIT_SEQNO);
  }
  rtt = (double**)(malloc(LIMIT_SEQNO * sizeof(double)));
  for (int i = 0; i < LIMIT_SEQNO; i++){
    rtt[i] = malloc(2 * sizeof(double));
  }

  // INITIALIZE BUFFER AND WINDOW
  buffer_init();
  window_init();
} 

/* called from layer 3, when a packet arrives for layer 4 at B*/
void
B_input (packet)
    struct pkt packet;
{

  // PACKET RECEIVED BY B
  int r_checksum = packet.checksum;
  int r_acknum = packet.acknum;
  int r_seqnum = packet.seqnum;
  char r_data[20];
  strncpy(r_data, packet.payload, 20);

  struct pkt *bpacket = malloc(sizeof(struct pkt));
  bpacket->acknum = r_seqnum;
  bpacket->seqnum = r_seqnum;
  bpacket->checksum = compute_checksum(r_seqnum, r_seqnum, r_data);
  strncpy(bpacket->payload, r_data, 20);

  //memcpy(bpacket, &packet, sizeof(struct pkt));
 
  if (TRACE >= 1){
    printf("PACKET %d RECEIVED BY B\n", r_seqnum);
  }

  int computed_checksum = compute_checksum(r_seqnum, r_acknum, r_data);

  // CHECK IF PACKET IS CORRUPTED
  if (r_checksum == computed_checksum){

      int index = B_window_index(r_seqnum);

      // PACKET IS IN CORRECT SEQUENCE
       if (r_seqnum == B_PACKETNUM){

         // JUST SEND IT
         B_ack(packet);
         B_add_window(bpacket);
         B_print_window();

      // CHECK IF PACKET IS A DUPLICATE
      } else if (index > -1){

        struct pkt *ack = B_WINDOW[index];
        tolayer3(B, *ack);
        B_ACKS++;

        if (TRACE >= 2){
          printf("Packet %d already received! Resending ack %d\n", r_seqnum, ack->acknum);
        }
        return;

      // PACKET IS OUT OF ORDER
      } else {

        if (TRACE >= 1){
          // OTHERWISE, IGNORE
          printf("ERROR: packet received out of order; SEQ NUM %d and EXPECTED NUM %d\n", r_seqnum, B_PACKETNUM);
        }   
        return;
      }
  } else {

    if (TRACE >= 1){
      CORRUPTED_PACKETS++;
      printf("ERROR: packet %d is corrupted\n", r_seqnum);
    }
  }

  return;
}


/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void
B_init (void)
{
  B_window_init();
  B_PACKETNUM = 0;
  B_INDEX = 0;
  return;

} 

/* called at end of simulation to print final statistics */
void Simulation_done()
{

    float lost = (float)(RETRANSMISSIONS - CORRUPTED_PACKETS) / (float)((ORIGINAL_PACKETS + RETRANSMISSIONS) + B_ACKS);
    float cor_rat = (float)CORRUPTED_PACKETS / (float)((ORIGINAL_PACKETS + RETRANSMISSIONS) + B_ACKS - (RETRANSMISSIONS - CORRUPTED_PACKETS));
    float x = (float)NET_TIME / (float)PACKETS_DELIVERED;
    float y = (float)ONESHOT_TIME / (float)PACKETS_DELIVERED;
 
    /* TO PRINT THE STATISTICS, FILL IN THE DETAILS BY PUTTING VARIBALE NAMES. DO NOT CHANGE THE FORMAT OF PRINTED OUTPUT */
    printf("\n\n===============STATISTICS======================= \n\n");
    printf("Number of original packets transmitted by A: %d\n", ORIGINAL_PACKETS);
    printf("Number of retransmissions by A: %d\n", RETRANSMISSIONS);
    printf("Number of data packets delivered to layer 5 at B: %d\n", PACKETS_DELIVERED);
    printf("Number of ACK packets sent by B: %d\n", B_ACKS);
    printf("Number of corrupted packets: %d\n", CORRUPTED_PACKETS);
    printf("Ratio of lost packets: %f\n", lost);
    printf("Ratio of corrupted packets: %f\n", cor_rat);
    printf("Average RTT: %f\n", y);
    printf("Average communication time: %f\n", x);
    printf("==================================================");
    
    /* PRINT YOUR OWN STATISTIC HERE TO CHECK THE CORRECTNESS OF YOUR PROGRAM */
    printf("\nEXTRA: \n");
    /* EXAMPLE GIVEN BELOW */
    /* printf("Example statistic you want to check e.g. number of ACK packets received by A : <YourVariableHere>"); */
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
******************************************************************/


struct event {
   double evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* Advance declarations. */
void init(void);
void generate_next_arrival(void);
void insertevent(struct event *p);


/* possible events: */
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1


int TRACE = 0;              /* for debugging purpose */
int fileoutput; 
double time_now = 0.000;
int WINDOW_SIZE;
int LIMIT_SEQNO;
double RXMT_TIMEOUT;
double lossprob;            /* probability that a packet is dropped  */
double corruptprob;         /* probability that one bit is packet is flipped */
double lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/
int nsim = 0;
int nsimmax = 0;
unsigned int seed[5];         /* seed used in the pseudo-random generator */

int
main(int argc, char **argv)
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;

   int i,j;

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
        time_now = eventptr->evtime;    /* update time to next event time */
        if (eventptr->evtype == FROM_LAYER5 ) {
            generate_next_arrival();   /* set up future arrival */
            /* fill in msg to give with string of same letter */
	    j = nsim % 26;
	    for (i=0;i<20;i++)
	      msg2give.data[i]=97+j;
	    msg2give.data[19]='\n';
	    nsim++;
	    if (nsim==nsimmax+1)
	      break;
	    A_output(msg2give);
	}
          else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
	    for (i=0;i<20;i++)
	      pkt2give.payload[i]=eventptr->pktptr->payload[i];
            if (eventptr->eventity ==A)      /* deliver packet by calling */
               A_input(pkt2give);            /* appropriate entity */
            else
               B_input(pkt2give);
            free(eventptr->pktptr);          /* free the memory for packet */
            }
          else if (eventptr->evtype ==  TIMER_INTERRUPT) {
	    A_timerinterrupt();
             }
          else  {
             printf("INTERNAL PANIC: unknown event type \n");
             }
        free(eventptr);
   }
terminate:
   Simulation_done(); /* allow students to output statistics */
   printf("Simulator terminated at time %.12f\n",time_now);
   return (0);
}


void
init(void)                         /* initialize the simulator */
{
  int i=0;
  printf("----- * Network Simulator Version 1.1 * ------ \n\n");
  printf("Enter number of messages to simulate: ");
  scanf("%d",&nsimmax);
  printf("Enter packet loss probability [enter 0.0 for no loss]:");
  scanf("%lf",&lossprob);
  printf("Enter packet corruption probability [0.0 for no corruption]:");
  scanf("%lf",&corruptprob);
  printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
  scanf("%lf",&lambda);
  printf("Enter window size [>0]:");
  scanf("%d",&WINDOW_SIZE);
  LIMIT_SEQNO = WINDOW_SIZE*2; // set appropriately; here assumes SR
  printf("Enter retransmission timeout [> 0.0]:");
  scanf("%lf",&RXMT_TIMEOUT);
  printf("Enter trace level:");
  scanf("%d",&TRACE);
  printf("Enter random seed: [>0]:");
  scanf("%d",&seed[0]);
  for (i=1;i<5;i++)
    seed[i]=seed[0]+i;
  fileoutput = open("OutputFile", O_CREAT|O_WRONLY|O_TRUNC,0644);
  if (fileoutput<0) 
    exit(1);
  ntolayer3 = 0;
  nlost = 0;
  ncorrupt = 0;
  time_now=0.0;                /* initialize time to 0.0 */
  generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* mrand(): return a double in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/*     modified by Chong Wang on Oct.21,2005                                */
/****************************************************************************/
int nextrand(int i)
{
  seed[i] = seed[i]*1103515245+12345;
  return (unsigned int)(seed[i]/65536)%32768;
}

double mrand(int i)
{
  double mmm = 32767;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  double x;                   /* individual students may need to change mmm */
  x = nextrand(i)/mmm;            /* x should be uniform in [0,1] */
  if (TRACE==0)
    printf("%.16f\n",x);
  return(x);
}


/********************* EVENT HANDLINE ROUTINES ***restart_****/
/*  The next set of routines handle the event list   */
/*****************************************************/
void
generate_next_arrival(void)
{
   double x,log(),ceil();
   struct event *evptr;
   //   char *malloc(); commented out by matta 10/17/2013

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

   x = lambda*mrand(0)*2;  /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time_now + x;
   evptr->evtype =  FROM_LAYER5;
   evptr->eventity = A;
   insertevent(evptr);
}

void
insertevent(p)
   struct event *p;
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %f\n",time_now);
      printf("            INSERTEVENT: future time will be %f\n",p->evtime);
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

void
printevlist(void)
{
  struct event *q;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
 struct event *q /* ,*qold */;
 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time_now);
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


void
starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
double increment;
{

 struct event *q;
 struct event *evptr;
 // char *malloc(); commented out by matta 10/17/2013

 if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",time_now);
 /* be nice: check to see if timer is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
   for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }

/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time_now + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
}


/************************** TOLAYER3 ***************/
void
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 // char *malloc(); commented out by matta 10/17/2013
 double lastime, x;
 int i;


 ntolayer3++;

 /* simulate losses: */
 if (mrand(1) < lossprob)  {
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
 for (i=0;i<20;i++)
   mypktptr->payload[i]=packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
          mypktptr->acknum,  mypktptr->checksum);
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
 lastime = time_now;
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
 for (q=evlist; q!=NULL ; q = q->next)
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) )
      lastime = q->evtime;
 evptr->evtime =  lastime + 1 + 9*mrand(2);



 /* simulate corruption: */
 /* modified by Chong Wang on Oct.21, 2005  */
 if (mrand(3) < corruptprob)  {
    ncorrupt++;
    if ( (x = mrand(4)) < 0.75)
       mypktptr->payload[0]='?';   /* corrupt payload */
      else if (x < 0.875)
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

void
tolayer5(datasent)
  char datasent[20];
{
  write(fileoutput,datasent,20);
}
