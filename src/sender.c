#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include "stp.h"

#define STDIN 0

// time delay function
void delay(int nb_seconds) {
    int ms = 1000 * nb_seconds;
    clock_t start_time = clock();
    while (clock() < start_time + ms)
        ;
}



int main(int argc, char *argv[])
{
    if(argc != 15)
    {
        printf("sender: %s [IP address] [message]\n", argv[0]);
        exit(1);
    }
    
    char *hostname = argv[1];
    int portnum = atoi(argv[2]);
    char *filename = argv[3];
    int MWS = atoi(argv[4]);
    int MSS = atoi(argv[5]);
    float gamma = atof(argv[6]);
    float pDrop = atof(argv[7]);
    float pDuplicate = atof(argv[8]);
    float pCorrup = atof(argv[9]);
    float pOrder = atof(argv[10]);
    int maxOrder = atoi(argv[11]);
    float pDelay = atof(argv[12]);
    int maxDelay = atoi(argv[13]);
    int seed = atoi(argv[14]);
    

    // seed for random numbers
    srand(seed);
    
    // summary parameters
    int FileSize = 0, SegTran = 0, SegPLD = 0, SegDrop = 0, SegCorr = 0, SegReord = 0, SegDup = 0, SegDelay = 0, RetranTimeout = 0, RetranFast = 0, DupAck =0;
    FILE *fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);
    fclose(fp);
    
    //clear log file if exists;
    clearlog("Sender_log.txt");
    
    //start timer
    double secs;
    struct timeval start, stop, timeout;
    gettimeofday(&start, NULL);
    
    // create socket
    socket_t sockfd;
    struct sockaddr_in server, peer;
    socklen_t socksize = sizeof(server);
    if((sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("socket() error\n");
        exit(1);
    }
    // set up socket information
    memset(&server, '\0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(portnum);
    server.sin_addr.s_addr = inet_addr(hostname);
    
    
    /*************** handshake with server ***************/
    bool inithandshake = false;
    packet handpack = newpacket(0);
    packet handackpack = newpacket(0);
    set_SYN(handpack);
    if(stp_sendto(sockfd, handpack, handpack->header.seg_len, 0, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Unable to send SYN packet to server for handshake\n");
        exit(1);
    }
    else
    {
        SegTran++;
        gettimeofday(&stop, NULL);
        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
        writelog("Sender_log.txt", "snd", secs, handpack);
    }
    if(stp_recvfrom(sockfd, handackpack, handackpack->header.seg_len, 0, (struct sockaddr *)&peer, &socksize) < 0) {
        perror("recv from error!\n");
    }
    gettimeofday(&stop, NULL);
    secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
    writelog("Sender_log.txt", "rcv", secs, handackpack);
    if(is_SYN(handackpack) && is_ACK(handackpack) && handackpack->header.ack_num == handpack->header.seq_num + 1){
        clr_SYN(handpack);
        set_ACK(handpack);
        handpack = set_acknum(handpack, handackpack->header.seq_num + 1);
        handpack = set_seqnum(handpack, handackpack->header.ack_num);
        if(stp_sendto(sockfd, handpack, handpack->header.seg_len, 0, (struct sockaddr *)&server, sizeof(server)) < 0){
            perror("Unable to handshake with server\n");
        } else{
            SegTran++;
            gettimeofday(&stop, NULL);
            secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
            writelog("Sender_log.txt", "snd", secs, handpack);
            inithandshake = true;
            printf("Handshake success!\n");
        }
    }
    if(inithandshake) {
        if(stp_connect(sockfd, &server, sizeof(server))==0)
            printf("Now connected to server(%s:%d).\n\n", hostname, portnum);
    }
    freepacket(handpack);
    freepacket(handackpack);
    
    /*************** send data to server ***************/
    // create send buffer
    Block senderbuffer = newBlock();
    senderbuffer = CreateFromFile(filename, 1, MSS);
    packet DataAckPack = newpacket(0);                  // pack to receive ack from server
    packet corrPack = newpacket(MSS);                   // corrupt packet
    //printf("the new block size is %d.\n", senderbuffer->size);
    
    // sending with maximum window size;
    int last_ack_seq = 0, dupack_rec_times = 0, target_ack_seq;
    bool re_tran_fast = false, re_tran_timeout = false;;
    target_ack_seq = FileSize - FileSize % MSS + 1;     // sequence no. of the last packet of data
    // initialize start and end of window for first time transmit
    int start_seq = 1, end_seq,retran_seq, delay_seq = 0;
    end_seq = 1 + (MWS / MSS - 1) * MSS;
    
    // TimeoutInterval
    float EstRTT = 0.5, DevRTT = 0.25;
    float TimeOutInterval;
    double SampleRTT, listentime = 0;
    
    // transmit data packets
    printf("Start to transfer files.\n");
    struct timeval time_send, time_ack,start_listen, end_listen, delay_start, delay_end;
    while(last_ack_seq != target_ack_seq)     // when last data packet is acknowledged, end the transfer for data.
    {
        gettimeofday(&start_listen, NULL);    // reset the timer when transmit a packet
        TimeOutInterval = EstRTT + gamma * DevRTT;
        //printf("Timeout Interval is %fs\n", TimeOutInterval);
        // end for the window size is updated whenever last ack seq changed;
        end_seq = last_ack_seq + (MWS / MSS) * MSS;
        if (end_seq > target_ack_seq)
            end_seq = target_ack_seq;
        
        // send the window.
        if(re_tran_fast || re_tran_timeout)     // re transmit
        {
            //printf("retransmission whole window with Go-Back-N!\n");
            // reset the retransmit sequence to send the sequence in ack
            if(last_ack_seq == 0)
                retran_seq = 1;
            else
                retran_seq = last_ack_seq + MSS;
            
            if(re_tran_fast){
                RetranFast++;
                printf("Fast retransmit!\n");
            }
            if(re_tran_timeout){
                RetranTimeout++;
                printf("Timeout retransmit\n");
            }
            packet packtosend = BufferRead_Seq(senderbuffer, retran_seq);
            if(stp_send(sockfd, packtosend, packtosend->header.seg_len, 0) > 0){
                SegTran++;
                //printf("@@@ retransmit datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
            }
            gettimeofday(&stop, NULL);
            secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
            writelog("Sender_log.txt", "snd/RXT", secs, packtosend);
            //start_seq += MSS;
            re_tran_fast = false;
            re_tran_timeout = false;
        }
        else            // normal transmit go through PLD
        {
            gettimeofday(&time_send, NULL);        // normal Transmit calculate sampleRTT
            int hold_seq = 0;
            while(start_seq <= end_seq)
            {
                SegTran++;
                // send reorder
                if(hold_seq != 0 && start_seq == (1+maxOrder) * MSS + hold_seq){
                    packet packtosend = BufferRead_Seq(senderbuffer, hold_seq);
                    if(stp_send(sockfd, packtosend, packtosend->header.seg_len, 0) > 0)
                        SegReord++;
                    gettimeofday(&stop, NULL);
                    secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                    writelog("Sender_log.txt", "snd/dely", secs, packtosend);
                    hold_seq = 0;
                    break;
                }
                
                packet packtosend = BufferRead_Seq(senderbuffer, start_seq);
                //printf("~~~datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
                int Re_PLD = cal_PLD(pDrop, pDuplicate, pCorrup, pOrder, pDelay);
                //printf("PLD result is: %d\n", Re_PLD);
                SegPLD++;
                switch(Re_PLD)
                {
                    case 0:             // normal case send
                        stp_send(sockfd, packtosend, packtosend->header.seg_len, 0);
                        gettimeofday(&stop, NULL);
                        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                        writelog("Sender_log.txt", "snd", secs, packtosend);
                        //printf("@@@nomal datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
                        break;
                    case 1:             // drop packet
                        SegDrop++;
                        gettimeofday(&stop, NULL);
                        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                        writelog("Sender_log.txt", "drop", secs, packtosend);
                        //printf("@@@drop datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
                        break;
                    case 2:             // duplicate packet
                        SegDup++;
                        SegTran++;
                        stp_send(sockfd, packtosend, packtosend->header.seg_len, 0);
                        gettimeofday(&stop, NULL);
                        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                        writelog("Sender_log.txt", "snd/dup", secs, packtosend);
                        stp_send(sockfd, packtosend, packtosend->header.seg_len, 0);
                        gettimeofday(&stop, NULL);
                        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                        writelog("Sender_log.txt", "snd/dup", secs, packtosend);
                        //printf("@@@dup datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
                        break;
                    case 3:             // corrupted
                        SegCorr++;
                        memcpy(corrPack, packtosend, packtosend->header.seg_len);
                        corrPack->header.seq_num = packtosend->header.seq_num ^ 1;
                        stp_send(sockfd, corrPack, corrPack->header.seg_len, 0);
                        gettimeofday(&stop, NULL);
                        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                        writelog("Sender_log.txt", "snd/corr", secs, corrPack);
                        //printf("@@@corrupt datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
                        break;
                    case 4:             // re-order
                        if (hold_seq == 0)
                            hold_seq = start_seq;
                        else            // ignore the re-order if there is already re-order waiting
                        {
                            stp_send(sockfd, packtosend, packtosend->header.seg_len, 0);
                            gettimeofday(&stop, NULL);
                            secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                            writelog("Sender_log.txt", "snd", secs, packtosend);
                        }
                        break;
                    case 5:             // delay
                        SegDelay++;
                        gettimeofday(&delay_start, NULL);
                        delay_seq = start_seq;
                        break;
                        //delay(maxDelay);
                        //printf("@@@delay datapack: seq(%d), ack(%d), flags(%d), size(%d)\n", packtosend->header.seq_num, packtosend->header.ack_num, packtosend->header.flag, packtosend->header.seg_len);
                }
                start_seq += MSS;
            }
        }
        // send delay sequence
        if (delay_seq != 0)
        {
            packet packtosend = BufferRead_Seq(senderbuffer, delay_seq);
            while(delay_seq != 0) {
                gettimeofday(&delay_end, NULL);
                secs = (double)(delay_end.tv_usec - delay_start.tv_usec) / 1000 + (double)(delay_end.tv_sec - delay_start.tv_sec) * 1000;
                if(secs > maxDelay){
                    stp_send(sockfd, packtosend, packtosend->header.seg_len, 0);
                    gettimeofday(&stop, NULL);
                    secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                    writelog("Sender_log.txt", "snd/dely", secs, packtosend);
                    delay_seq = 0;
                }
            }
        }
        
        
        // recv for data packet ACKs from server
        while(last_ack_seq != target_ack_seq)
        {
            // if needs retransmit by 3 DA
            if (dupack_rec_times >=3)
            {
                //printf(">>>fast retran!!!\n");
                re_tran_fast = true;
                dupack_rec_times = 0;
                break;
                
            }
            
            // if time out during listening
            gettimeofday(&end_listen, NULL);
            listentime = (double)(end_listen.tv_usec - start_listen.tv_usec) / 1000000 + (double)(end_listen.tv_sec - start_listen.tv_sec);
            if(listentime > TimeOutInterval){
                //printf(">>>timeout!!!\n");
                re_tran_timeout = true;
                dupack_rec_times = 0;
                break;
            }
            
            
            timeout.tv_sec = 0;
            timeout.tv_usec = 50000;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
            if(recv(sockfd, DataAckPack, DataAckPack->header.seg_len, 0) == -1){
                //printf("$$$thread unable to recv from server!!!!\n");
                //break;
                continue;
            } else
            {
                //printf("####receive: seq(%d), ack(%d), ack(%d), flags(%d), size(%d)\n", DataAckPack->header.seq_num,DataAckPack->header.ack_num, DataAckPack->header.ack_num, DataAckPack->header.flag, DataAckPack->header.seg_len);
                gettimeofday(&stop, NULL);
                double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
                
                //printf("status: last(%d), dupack(%d), target(%d), header(%d)\n", last_ack_seq, dupack_rec_times, target_ack_seq, DataAckPack->header.seq_num);
                
                // if last ack is received duplicate.
                if(DataAckPack->header.ack_num > target_ack_seq) {
                    last_ack_seq = target_ack_seq;
                    dupack_rec_times = 0;
                    writelog("Sender_log.txt", "rcv", secs, DataAckPack);
                    gettimeofday(&time_ack, NULL);
                    SampleRTT = (double)(time_ack.tv_usec - time_send.tv_usec) / 1000000 + (double)(time_ack.tv_sec - time_send.tv_sec);
                    EstRTT = 0.875 * EstRTT + 0.125 * SampleRTT;
                    DevRTT = 0.75 * DevRTT + 0.25 * fabs(EstRTT - SampleRTT);
                    break;
                }
                else if(DataAckPack->header.ack_num - MSS == last_ack_seq){
                    dupack_rec_times += 1;
                    DupAck++;
                    writelog("Sender_log.txt", "rcv/DA", secs, DataAckPack);
                } else if(DataAckPack->header.ack_num == 1){
                    last_ack_seq = 0;
                    dupack_rec_times += 1;
                    DupAck++;
                    writelog("Sender_log.txt", "rcv/DA", secs, DataAckPack);
                }
                else {
                    last_ack_seq = DataAckPack->header.ack_num - MSS;
                    dupack_rec_times = 0;
                    writelog("Sender_log.txt", "rcv", secs, DataAckPack);
                    gettimeofday(&time_ack, NULL);
                    SampleRTT = (double)(time_ack.tv_usec - time_send.tv_usec) / 1000000 + (double)(time_ack.tv_sec - time_send.tv_sec);
                    EstRTT = 0.875 * EstRTT + 0.125 * SampleRTT;
                    DevRTT = 0.75 * DevRTT + 0.25 * fabs(EstRTT - SampleRTT);
                    break;
                }
            }
        }
    }
    freepacket(DataAckPack);
    freepacket(corrPack);
    printf("File has been transmitted to server.\n\n");
    
    // drop block
    dropBlock(senderbuffer);
    
 
    /*************** close the connection ***************/
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // send FIN to close the connection
    packet ClientFINPack = newpacket(0);
    set_FIN(ClientFINPack);
    ClientFINPack = set_seqnum(ClientFINPack, FileSize + 1);
    if(stp_send(sockfd, ClientFINPack, ClientFINPack->header.seg_len,0) > 0){
        SegTran++;
        printf("Request to close connection.\n");
    }
    else {
        perror("unable to send close request.\n");
    }
    gettimeofday(&stop, NULL);
    secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
    writelog("Sender_log.txt", "snd", secs, ClientFINPack);
    freepacket(ClientFINPack);
    
    //receive ack from server
    bool request_close = false;
    bool connection_end = false;
    packet ClientFINACKpack = newpacket(0);
    packet ServerFINACKpack = newpacket(0);
    while(!connection_end)
    {
        if(recvfrom(sockfd, ClientFINACKpack, ClientFINACKpack->header.seg_len, 0, (struct sockaddr *)&peer, &socksize) < 0 )
            perror("recv error!\n");
        //printf("^^receive: seq(%d), ack(%d), ack(%d), flags(%d), size(%d)\n", ClientFINACKpack->header.seq_num,ClientFINACKpack->header.ack_num, ClientFINACKpack->header.ack_num, ClientFINACKpack->header.flag, ClientFINACKpack->header.seg_len);
        gettimeofday(&stop, NULL);
        secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
        writelog("Sender_log.txt", "rcv", secs, ClientFINACKpack);
        if(is_ACK(ClientFINACKpack) && ClientFINACKpack->header.ack_num == FileSize + 2){
            printf("Receiver server's permission to close, about to close.\n");
            request_close = true;
        }
        else if(is_FIN(ClientFINACKpack) && request_close) {
            printf("Receive server's request to close.\n");
            
            set_ACK(ServerFINACKpack);
            ServerFINACKpack = set_seqnum(ServerFINACKpack, ClientFINACKpack->header.ack_num);
            ServerFINACKpack = set_acknum(ServerFINACKpack, ClientFINACKpack->header.seq_num + 1);
            gettimeofday(&stop, NULL);
            secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
            writelog("Sender_log.txt", "snd", secs, ServerFINACKpack);
            if (stp_send(sockfd, ServerFINACKpack, ServerFINACKpack->header.seg_len, 0) > 0){
                printf("Connection Closed!\n");
                SegTran++;
            }
            connection_end = true;
        }
    }
    freepacket(ServerFINACKpack);
    freepacket(ClientFINACKpack);
    
    // close socket
    close(sockfd);
    
    // write summary to log
    writeSummarySender("Sender_log.txt", FileSize, SegTran, SegPLD, SegDrop, SegCorr, SegReord, SegDup, SegDelay, RetranTimeout, RetranFast, DupAck);

}
