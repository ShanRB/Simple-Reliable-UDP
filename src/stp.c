#include "stp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//PLD return values definition;
#define NORMAL 0;
#define DROP 1;
#define DUPLICATE 2;
#define CORRUPT 3;
#define REORDER 4;
#define DELAY 5;



socket_t stp_socket(int domain, int type, int protocol){
    if (type != SOCK_DGRAM) {
        perror("STP is UDP-based protocol.");
        return -1;
    }
    return socket(domain, type, protocol);
}

int stp_bind(socket_t sockfd, struct sockaddr_in *addr, int addrlen) {
    return bind(sockfd, (struct sockaddr *)addr, addrlen);
}

int stp_connect(socket_t sockfd, struct sockaddr_in *addr, int addrlen){
    return connect(sockfd, (struct sockaddr *)addr, addrlen);
}

int stp_listen(socket_t sockfd, int backlog){
    return listen(sockfd, backlog);
}

int stp_accept(socket_t sockfd, struct sockaddr_in *addr, int *addrlen){
    return accept(sockfd, (struct sockaddr *)addr, (unsigned int *)addrlen);
}

int stp_send(socket_t sockfd, packet p, size_t len, int flags){
    return (int)send(sockfd, p, len, flags);
}

int stp_sendto(socket_t sockfd, void *buf, size_t len, int flags,
            struct sockaddr *dest_addr, socklen_t addrlen){
    return (int)sendto(sockfd, buf, len, flags, (struct sockaddr *)dest_addr, addrlen);
}

int stp_recv(socket_t sockfd, void *buf, size_t len, int flags){
    return (int)recv(sockfd, buf, len, flags);
}

int stp_recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen){
    return (int)recvfrom(sockfd, buf, len, flags, (struct sockaddr *)src_addr, addrlen);
}


packet receiver_ack_data_packet(Block b) {
    int ack_num = 0;
    packet replyp = newpacket(0);
    set_ACK(replyp);
    replyp = set_seqnum(replyp, 1);
    // find ack number
    if (b->size == 0)
        replyp = set_acknum(replyp, 1);
    else {
        Node *node = b->head;
        int MSS = b->head->p->header.seg_len - sizeof(headerRep);
        int i;
        for(i = 0; i < b->size; i++) {
           if(node->p->header.seq_num != i * MSS + 1){
               //printf("no match at %d, nodeseq(%d), ith seq(%d)\n", i,node->p->header.seq_num, i*MSS+1);
                ack_num = i * MSS + 1;
                break;
           }
           else if(node->next == NULL) {
               //printf("reach end at %d, nodeseq(%d), ith seq(%d)\n", i,node->p->header.seq_num, i*MSS+1);
               ack_num = node->p->header.seq_num + node->p->header.seg_len - sizeof(headerRep);
               break;
           }
            node = node->next;
        }
        replyp = set_acknum(replyp, ack_num);
    }
    return replyp;
}



/**************** log file functions ****************/

int cal_PLD(float pDrop, float pDuplicate, float pCorrupt, float pOrder, float pDelay) {
float x = rand()/((float)(RAND_MAX)+1);
//printf("x(%f): drop(%.1f) dup(%.1f) cor(%.1f) ord(%.1f) delay(%.1f)\n",x,pDrop,pDuplicate,pCorrupt,pOrder,pDelay);
    if(x < pDrop){
        return DROP;
        
    } else {
        x = rand()/((float)(RAND_MAX)+1);
        if(x < pDuplicate){
            return DUPLICATE;
            
        } else {
            x = rand()/((float)(RAND_MAX)+1);
            if(x < pCorrupt) {
                return CORRUPT;
                
            } else {
                x = rand()/((float)(RAND_MAX)+1);
                if(x < pOrder) {
                    return REORDER;
                    
                } else {
                    x = rand()/((float)(RAND_MAX)+1);
                    if(x < pDelay){
                        return DELAY;
                    } else{
                        return NORMAL;
                    }
                }
                
            }
            
        }
        
    }
 }





/**************** log file functions ****************/

// create log file if not exists, clear log file if exists
void clearlog(char *filename) {
    FILE *fp = fopen(filename, "w");
    fclose(fp);
}
// write into log file
void writelog(char *logfilename, char *event, float time, packet p) {
    FILE *fp = fopen(logfilename, "a");
    //write event in log
    fprintf(fp, "%-10s", event);
    fprintf(fp, " \t");
    // write time in log
    fprintf(fp,"%10.2f",time);
    fprintf(fp, " \t");
    // write data type in log
    if (is_SYN(p))
        fprintf(fp, "%s", "S");
    if (is_ACK(p))
        fprintf(fp, "%s", "A");
    if (is_FIN(p))
        fprintf(fp, "%s", "F");
    if (is_DTA(p))
        fprintf(fp, "%s", "D");
    fprintf(fp, " \t");
    fprintf(fp, " \t");
    //write sequence number in log
    int seq = p->header.seq_num;
    fprintf(fp,"%10d", seq);
    fprintf(fp, " \t");
    //write no. of bytes of data in log
    int datasize = p->header.seg_len - sizeof(headerRep);
    fprintf(fp,"%10d", datasize);
    fprintf(fp, " \t");
    //write ack number in log
    int ack_num = p->header.ack_num;
    fprintf(fp,"%10d", ack_num);
    fprintf(fp, " \t");
    //end of line
    fprintf(fp, "\n");
    fclose(fp);
}

void writeSummaryReceiver(char *logfilename, int TotalData, int TotalSeg, int DataSeg, int DataBiterror, int DupDataSeg, int DupAck) {
    FILE *fp = fopen(logfilename, "a");
    fprintf(fp,"==============================================\n");
    fprintf(fp,"%-35s","Amount of data received (bytes)");
    fprintf(fp, "%10d\n", TotalData);
    fprintf(fp,"%-35s","Total Segments Received");
    fprintf(fp, "%10d\n", TotalSeg);
    fprintf(fp,"%-35s","Data segments received");
    fprintf(fp, "%10d\n", DataSeg);
    fprintf(fp,"%-35s","Data segments with Bit Errors");
    fprintf(fp, "%10d\n", DataBiterror);
    fprintf(fp,"%-35s","Duplicate data segments received");
    fprintf(fp, "%10d\n", DupDataSeg);
    fprintf(fp,"%-35s","Duplicate ACKs sent");
    fprintf(fp, "%10d\n", DupAck);
    fprintf(fp,"==============================================");
    fclose(fp);
}

void writeSummarySender(char *logfilename, int FileSize, int SegTran, int SegPLD, int SegDrop, int SegCorr, int SegReord, int SegDup, int SegDelay, int RetranTimeout, int RetranFast, int DupAck) {
    FILE *fp = fopen(logfilename, "a");
    fprintf(fp,"=============================================================\n");
    fprintf(fp,"%-50s","Size of the file (in Bytes)");
    fprintf(fp, "%10d\n", FileSize);
    fprintf(fp,"%-50s","Segments transmitted (including drop & RXT)");
    fprintf(fp, "%10d\n", SegTran);
    fprintf(fp,"%-50s","Number of Segments handled by PLD");
    fprintf(fp, "%10d\n", SegPLD);
    fprintf(fp,"%-50s","Number of Segments Dropped");
    fprintf(fp, "%10d\n", SegDrop);
    fprintf(fp,"%-50s","Number of Segments Corrupted");
    fprintf(fp, "%10d\n", SegCorr);
    fprintf(fp,"%-50s","Number of Segments Re-ordered");
    fprintf(fp, "%10d\n", SegReord);
    fprintf(fp,"%-50s","Number of Segments Duplicated");
    fprintf(fp, "%10d\n", SegDup);
    fprintf(fp,"%-50s","Number of Segments Delayed");
    fprintf(fp, "%10d\n", SegDelay);
    fprintf(fp,"%-50s","Number of Retransmissions due to timeout");
    fprintf(fp, "%10d\n", RetranTimeout);
    fprintf(fp,"%-50s","Number of Fast Retransmissions");
    fprintf(fp, "%10d\n", RetranFast);
    fprintf(fp,"%-50s","Number of Duplicate Acknowledgements received");
    fprintf(fp, "%10d\n", DupAck);
    fprintf(fp,"=============================================================");
    fclose(fp);
}

