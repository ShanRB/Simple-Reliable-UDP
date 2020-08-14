// header file for STP implementation
#include <netinet/in.h>
#include "buffer.h"


typedef int socket_t;

// STP functions based on UDP
socket_t stp_socket(int , int , int );
int stp_bind(socket_t, struct sockaddr_in *, int);
int stp_connect(socket_t, struct sockaddr_in *, int);
int stp_listen(socket_t, int);
int stp_accept(socket_t, struct sockaddr_in *, int *);
int stp_send(socket_t, packet, size_t, int);
int stp_sendto(socket_t, void *, size_t, int, struct sockaddr *, socklen_t);
int stp_recv(socket_t, void *, size_t, int );
int stp_recvfrom(socket_t, void *, size_t, int, struct sockaddr *, socklen_t *);
packet receiver_ack_data_packet(Block);

//PLD
int cal_PLD(float, float, float, float, float);

// log functions
void clearlog(char *);
void writelog(char *, char *, float, packet);
void writeSummaryReceiver(char *, int, int, int, int, int, int);
void writeSummarySender(char *, int, int, int, int, int, int, int, int, int, int, int);
