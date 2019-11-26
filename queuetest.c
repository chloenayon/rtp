#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <string.h>

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

struct pkt *(WINDOW[WINDOW_SIZE]);

void window_init(){
  for (int i = 0; i < WINDOW_SIZE; i++){
    WINDOW[i] = malloc(sizeof(struct pkt));
  }
}

void add_window(){
  //strcpy(WINDOW[WINDOW_INDEX]->data, m.data);
  return;
}

void remove_window(){
  return;
}

void print_window(){
  printf("WINDOW:\n");
  printf("[");
  for (int i = 0; i < WINDOW_SIZE; i++){
    printf("%s, ", BUFFER[i].data);
  }
  printf("]\n");
  return;
}

int main(int argc, char **argv) {

    struct msg m;
    strcpy(m.data, "a");

    struct msg m2;
    strcpy(m2.data, "b");

    struct msg m3;
    strcpy(m3.data, "c");

    struct msg m4;
    strcpy(m4.data, "d");

    enqueue(m);
    print_queue();
    enqueue(m2);
    print_queue();
    enqueue(m3);
    print_queue();
    dequeue();
    print_queue();
}