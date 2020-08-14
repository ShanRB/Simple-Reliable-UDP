#include <inttypes.h>
#include <stdbool.h>

#define FLAG_INI 0b00000000
#define FLAG_DTA 0b00001000
#define FLAG_FIN 0b00000100
#define FLAG_ACK 0b00000010
#define FLAG_SYN 0b00000001

//#define header_size 16


typedef unsigned char flag_t;

typedef struct headerRep{
    uint32_t    seq_num;
    uint32_t    ack_num;
    uint16_t    seg_len;
    uint16_t    checksum;
    flag_t      flag;
} headerRep;

typedef struct packet{
    headerRep header;
    char data[1024];
} packetRep;

typedef packetRep *packet;


packet newpacket( int);
packet packet_resize(packet, int);
packet set_seqnum (packet, uint32_t);
packet set_acknum (packet, uint32_t);
packet set_data (packet, char *, int);
uint16_t cal_checksum(packet);
bool eval_checksum(packet);
bool test_eval_checksum(packet);
void set_DTA(packet);
void set_FIN(packet);
void set_ACK(packet);
void set_SYN(packet);
void clr_DTA(packet);
void clr_FIN(packet);
void clr_ACK(packet);
void clr_SYN(packet);
bool is_DTA(packet);
bool is_FIN(packet);
bool is_ACK(packet);
bool is_SYN(packet);
void freepacket(packet);
