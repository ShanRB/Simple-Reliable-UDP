#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

Block newBlock() {
    Block newb = malloc(sizeof(BlockRep));
    assert(newb != NULL);
    newb->head = NULL;
    newb->size = 0;
    newb->amt_dup = 0;
    return newb;
}

// drop the Block
void dropBlock(Block b) {
    Node *node = b->head;
    while(node != NULL) {
        Node *temp = node->next;
        freepacket(node->p);
        free(node);
        node = temp;
        b->size --;
    }
    free(b);
}

// create node from packet
Node *makeNode(packet pack) {
    //printf("1.makenode\n");
    Node *newnode = malloc(sizeof(Node));
    //printf("2.makenode----sizeof(%lu)\n", sizeof(newnode->p));
    assert(newnode != NULL);
    //printf("3.makenode\n");
    newnode->p = pack;
    //printf("4.makenode\n");
    newnode->next = NULL;
    //printf("makenode success\n");
    return newnode;
}

bool inBuffer(Block b, packet pack) {
    Node *node = b->head;
    while(node != NULL) {

        if (node->p->header.seq_num == pack->header.seq_num) {
            b->amt_dup ++;
            return true;
        }
        node = node->next;
    }
    return false;
}

// write p into Block assending by sequence number
Block BufferWrite(Block b, packet pack) {
    // if pack already in buffer, ignore it
    if(inBuffer(b, pack))
        return b;
    
    Node *node = b->head;
    //Node *newnode = makeNode(pack);
    Node *newnode = malloc(sizeof(Node));
    newnode->next = NULL;
    newnode->p = pack;
    
    // empty block
    if (node == NULL) {
        b->head =newnode;
        b->size++;
    }
    else if(pack->header.seq_num < node->p->header.seq_num){
        b->head = newnode;
        newnode->next = node;
        b->size++;
    }
//    else if(node->next == NULL && node->p->header.seq_num < pack->header.seq_num) {
//        node->next = newnode;
//        b->size++;
//    }
    else {
        while(node->next != NULL) {
            if (node->next->p->header.seq_num < pack->header.seq_num) {
                node = node->next;
            }else {
                newnode->next = node->next;
                node->next = newnode;
                b->size++;
                break;
            }
        }
        if(node->next == NULL) {
            //printf("$$$$inserted in the end.\n");
            node->next = newnode;
            b->size++;
        }
    }
    return b;
}

// read the first Block packet
packet BufferRead(Block b){
    packet pack;
    Node *node = b->head;
    b->head = node->next;
    b->size--;
    pack = node->p;
    //free(node);
    return pack;
}


// copy packet with sequence number
packet BufferRead_Seq(Block b, int sequence) {
    Node *node = b->head;
    while(node != NULL) {
        if(node->p->header.seq_num == sequence)
            break;
        node = node->next;
    }
    return node->p;
}


Block CreateFromFile(char* filename, int sequence, int MSS) {
    Block newb = newBlock();
    FILE *fp = NULL;
    int nb_segments, i;
    char buf[MSS];
    
    fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    long int filesize = ftell(fp);
    rewind(fp);
    nb_segments = (int)(filesize / MSS);
    
    for(i = 0; i < nb_segments; i++) {
        packet datapack = newpacket(MSS);
        fseek(fp, i*MSS, SEEK_SET);
        fread(buf, 1, MSS, fp);
        datapack = set_data(datapack, buf, MSS);
        datapack = set_seqnum(datapack, sequence + MSS * i);
        datapack = set_acknum(datapack, 1);
        set_DTA(datapack);
        //printf("datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", datapack->header.seq_num, datapack->header.ack_num, datapack->header.flag, datapack->header.seg_len);
        datapack->header.checksum = 0;
        cal_checksum(datapack);
        newb = BufferWrite(newb, datapack);
    }
    int dataleft = filesize % MSS;
    if (dataleft > 0) {
        char buf2[dataleft];
        //printf("dataleft: %d\n", dataleft);
        packet dataleftpack = newpacket(dataleft);
        fseek(fp, nb_segments*MSS, SEEK_SET);
        fread(buf2, 1, dataleft, fp);
        dataleftpack = set_data(dataleftpack, buf2, MSS);
        dataleftpack = set_seqnum(dataleftpack, sequence + MSS * i);
        set_DTA(dataleftpack);
        dataleftpack->header.checksum = 0;
        cal_checksum(dataleftpack);
        //printf("datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", dataleftpack->header.seq_num, dataleftpack->header.ack_num, dataleftpack->header.flag, dataleftpack->header.seg_len);
        newb = BufferWrite(newb, dataleftpack);
    }
    fclose(fp);
    return newb;
}
