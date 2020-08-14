#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/time.h>
#include "stp.h"



int main(int argc, char *argv[]) {
    assert(argc == 3);
    int header_size = sizeof(headerRep);
    // get user inputs
    int port = atoi(argv[1]);
    char *finename = argv[2];
    
    // create summary parameters;
    int TotalData = 0, TotalSeg = 0, DataSeg = 0, DataBiterror = 0, DupDataSeg = 0, DupAck = 0;
    
    //clear log file if exists;
    char *logfilename = "Receiver_log.txt";
    clearlog(logfilename);
    
    // create socket variables
    socket_t sockfd;
    struct sockaddr_in server, client;
    socklen_t size = sizeof(struct sockaddr_in);
    //create socket
    if((sockfd = stp_socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Server failed to create STP socket.\n");
        exit(1);
    }
    // set up server information
    memset(&server, '\0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    // bind the socket
    if(stp_bind(sockfd, &server, sizeof(server)) == -1)
    {
        perror("Bind() error.\n");
        exit(1);
    }
    
    //start timer
    struct timeval start,stop;
    gettimeofday(&start, NULL);
    
    // do handshake
    bool handshake = false;
    packet rechandshake = newpacket(0);
    while(!handshake) {
        int rec_result = stp_recvfrom(sockfd, rechandshake, rechandshake->header.seg_len, 0, (struct sockaddr *)&client, &size);
        if (rec_result >=0) {
            TotalData += rec_result;        // Total data receive increase
            TotalSeg += 1;                  // segment receive increase by 1
            
            // write into log
            gettimeofday(&stop, NULL);
            double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
            writelog(logfilename, "rcv", secs, rechandshake);
            // if first SYN is received.
            if(is_SYN(rechandshake)) {
                printf("Receive handshake request from: <%s:%d>\n",inet_ntoa(client.sin_addr),htons(client.sin_port));
                // packet SYN ACK, seq = 0;
                packet ackpack = newpacket(0);
                set_SYN(ackpack);
                set_ACK(ackpack);
                ackpack = set_acknum(ackpack, 1);
                stp_sendto(sockfd, ackpack, ackpack->header.seg_len, 0, (struct sockaddr*)&client, size);
                gettimeofday(&stop, NULL);
                double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                writelog(logfilename, "snd", secs, ackpack);
                free(ackpack);
            }
            else if(rechandshake->header.flag == FLAG_ACK && rechandshake->header.ack_num == 0x00000001){
//                gettimeofday(&stop, NULL);
//                double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
//                writelog(logfilename, "rcv", secs, rechandshake);
                printf("Connected to client!\n");
                handshake = true;
            } else
                printf("handshake packet not received...\n");
        }
        else
            perror("STP: recv error!\n");
    }
freepacket(rechandshake);
    
    
    // receive data files
    Block recbuffer = newBlock();
    bool request_close = false;
    while(1) {
        //printf("ready for packet receiving....\n");
        packet packrec = newpacket(60000);
        //printf("packet size is-----%d\n", packrec->header.seg_len);
        int rec_result = stp_recvfrom(sockfd, packrec, packrec->header.seg_len, 0, (struct sockaddr *)&client, &size);
        if (rec_result < 0)
            perror("STP: recv error!\n");
        
        TotalData += rec_result;
        TotalSeg += 1;

        // resize the packet based on real size;
        packrec = packet_resize(packrec, packrec->header.seg_len);

        // write in the log for receiving
        gettimeofday(&stop, NULL);
        double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
        writelog(logfilename, "rcv", secs, packrec);
        
        // if input is data segment
        if(is_DTA(packrec))
        {
            // if corrupted packet received, ignore it.
            if(!eval_checksum(packrec))
            {
                printf("corrupted packet!\n");
                gettimeofday(&stop, NULL);
                double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                writelog(logfilename, "rcv/corr", secs, packrec);
                DataBiterror ++;
            } else  // not corrupted
            {
                DataSeg += 1;
                int next_seq = packrec->header.seq_num + packrec->header.seg_len - header_size;
                
                // write into buffer
                printf("*receive to write in buffer: seq(%d), ack(%d), flags(%d), size(%d)\n", packrec->header.seq_num, packrec->header.ack_num, packrec->header.flag, packrec->header.seg_len);
                recbuffer = BufferWrite(recbuffer, packrec);
                //printf("* buffer size : %d,  buffer dup: %d\n", recbuffer->size, recbuffer->amt_dup);
                
                // send ACKs for Data packet
                packet ackpack = receiver_ack_data_packet(recbuffer);
                printf(">ack datapack: ack(%d), flags(%d)\n", ackpack->header.ack_num, ackpack->header.flag);
                stp_sendto(sockfd, ackpack, ackpack->header.seg_len, 0, (struct sockaddr*)&client, size);
                gettimeofday(&stop, NULL);
                secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                if(next_seq == ackpack->header.ack_num)
                    writelog(logfilename, "snd", secs, ackpack);
                else {
                    writelog(logfilename, "snd/DA", secs, ackpack);
                    DupAck ++;
                }
                
                //free memory
                freepacket(ackpack);
            }
        }
        // if input is FIN segment
        else if(is_FIN(packrec)) {
            printf("*receive to write in buffer: seq(%d), ack(%d), flags(%d), size(%d)\n", packrec->header.seq_num, packrec->header.ack_num, packrec->header.flag, packrec->header.seg_len);
            printf("Receive close connection request from client...\n");
            request_close = true;
            packet ackpack = newpacket(0);
            set_ACK(ackpack);
            ackpack = set_seqnum(ackpack, 1);
            ackpack = set_acknum(ackpack, packrec->header.seq_num + 1);
            if(sendto(sockfd, ackpack, ackpack->header.seg_len, 0, (struct sockaddr*)&client, size) != -1) {
                //printf("FINACK is sent back to client.\n");
                gettimeofday(&stop, NULL);
                secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                writelog(logfilename, "snd", secs, ackpack);
                //send out FIN from server
                packet finpack = newpacket(0);
                set_FIN(finpack);
                finpack = set_seqnum(finpack, 1);
                finpack = set_acknum(finpack, ackpack->header.seq_num);
                gettimeofday(&stop, NULL);
                secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                writelog(logfilename, "snd", secs, finpack);
                if(stp_sendto(sockfd, finpack, finpack->header.seg_len, 0, (struct sockaddr*)&client, size) != -1)
                    printf("Server request to close\n");
                else
                    printf("unable to send FIN request.\n");
                freepacket(finpack);
                freepacket(ackpack);
            }
            else {
                printf("unable to send FINACK request.\n");
                break;
            }
        }
        else if(request_close && is_ACK(packrec)) {
            printf("Connection Closed!\n");
            break;
        }
        else {
            printf("unrecognized packet: seq(%d), ack(%d), flags(%d), size(%d)\n", packrec->header.seq_num, packrec->header.ack_num, packrec->header.flag, packrec->header.seg_len);
        }
        
    }
    // socket is closed.
    close(sockfd);
    
    // if transmisson is closed, write from buffer to file.
    FILE *fp = NULL;
    fp = fopen(finename, "wb");
    //printf("-----------write to file--------------\n");
    //printf("Receiver Buffer size is: %d\n", recbuffer->size);
    DupDataSeg = recbuffer->amt_dup;
    int i, bufsize = recbuffer->size;
    for(i = 0; i < bufsize; i++) {
        packet packtowrite = BufferRead(recbuffer);
        int size =packtowrite->header.seg_len - header_size;
        fwrite(packtowrite->data, 1, size, fp);
        freepacket(packtowrite);
    }
    dropBlock(recbuffer);
    printf("Successfully save to file %s.\n", finename);
    
    // close files and socket.
    fclose(fp);
    
    //write summary log
    writeSummaryReceiver(logfilename, TotalData, TotalSeg, DataSeg, DataBiterror, DupDataSeg, DupAck);
}
