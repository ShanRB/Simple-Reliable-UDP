#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include "packet.h"


uint16_t check_sum(uint16_t *p)
{
    int headerlength = sizeof(headerRep);       // how many bytes in the header
    uint32_t checksum = 0;
    // divide them by 2 bytes and add to checksum
    while (headerlength > 1) {
        checksum += *p++;
        headerlength -= 2;
    }
    // if still byte left, add that to checksum
    if (headerlength) {
        checksum += *(unsigned char *)p;
    }
    // add overflow back to checksum
    while (checksum >> 16) {
        checksum = (checksum >> 16) + (checksum & 0xffff);
    }
    return (uint16_t)(~checksum);
}

packet newpacket(int datasize) {
    packet newp = malloc(sizeof(packetRep)+datasize);
    newp->header.seq_num = 0x00000000;
    newp->header.ack_num = 0x00000000;
    newp->header.flag = FLAG_INI;
    newp->header.checksum = 0x0000;
    newp->header.seg_len = sizeof(headerRep) + datasize;
    return newp;
}
packet packet_resize(packet p, int datasize) {
    packet newp = malloc(sizeof(packetRep));
    memcpy(newp, p, datasize);
    free(p);
    return newp;
}

uint16_t cal_checksum(packet p) {
    // clear current checksum
    //p->header.checksum = 0;
    uint16_t cksum = check_sum((uint16_t *)&p->header);
    p->header.checksum = cksum;
    return cksum;
}

bool eval_checksum(packet p) {
    bool result;
    packet newp = newpacket(0);
    newp->header = p->header;
    newp->header.checksum = 0;
    result = (cal_checksum(newp) == p->header.checksum);
    freepacket(newp);
    return result;
}

bool test_eval_checksum(packet p) {
    return cal_checksum(p) == 0xffff;
}

packet set_seqnum (packet p, uint32_t num) {
    p->header.seq_num = num;
    return p;
}

packet set_acknum (packet p, uint32_t num) {
    p->header.ack_num = num;
    return p;
}

packet set_data (packet p, char *msg, int len) {
    memcpy(p->data, msg, len);
    return p;
}


// set flags
void set_DTA(packet p) {
    p->header.flag |= FLAG_DTA;
}
void set_FIN(packet p) {
    p->header.flag |= FLAG_FIN;
}
void set_ACK(packet p) {
    p->header.flag |= FLAG_ACK;
}
void set_SYN(packet p) {
    p->header.flag |= FLAG_SYN;
}

// clear flags
void clr_DTA(packet p) {
    p->header.flag &= 0b0111;
}
void clr_FIN(packet p) {
    p->header.flag &= 0b1011;
}
void clr_ACK(packet p) {
    p->header.flag &= 0b1101;
}
void clr_SYN(packet p) {
    p->header.flag &= 0b1110;
}

// check flags
bool is_DTA(packet p) {
    return (p->header.flag & FLAG_DTA) == FLAG_DTA;
}
bool is_FIN(packet p) {
    return (p->header.flag & FLAG_FIN) == FLAG_FIN;
}
bool is_ACK(packet p) {
    return (p->header.flag & FLAG_ACK) == FLAG_ACK;
}
bool is_SYN(packet p) {
    return (p->header.flag & FLAG_SYN) == FLAG_SYN;
}

void freepacket(packet p) {
    free(p);
};
