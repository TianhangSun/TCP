#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmath>
#include <ctime>
#include <chrono>  // for clock-based random number generation
#include <random>  // for normal distribution
#include <signal.h>
#include <sys/time.h>
#include <fstream>

#include "message.h"

#define MEAN 70.0
#define STRDEV 3.0

using namespace std;

struct itimerval tval;
Ack_MSG ack_msg; 
Data_MSG msg;
Data_MSG end_msg;
int	sample_interval;   // interval between sensor samples
int retrans_interval;  // interval between retransmissions
int maxtrans;
bool notdone = true;
int transcount = 0;
int udp_socket, tcp_socket ;
sockaddr_in client_udp_addr;
socklen_t slen = sizeof(sockaddr_in);
UDP_Cntr_MSG ctrl_msg;
bool not_acked = true;
int end_cnt = 0;
bool finish = false;
int end_sequence = -1;
int timeout;

void  retransmit();
ssize_t transmit(int sockfd, const void *buf, size_t len, int flags,
				 const struct sockaddr *dest_addr, socklen_t addrlen);
double drop_prob;  // chance that a packet is dropped


int main(int argc, char *argv[])
{
    struct timeval now;
    sockaddr_in client_tcp_addr;
    sockaddr_in server_tcp_addr;

    // get arguments from the command line
    if ( (argc != 5)){
	cout << "Usage: " << argv[0];
        cout << " - l <loss-rate> -t <timeout-value(in us)>" << endl;
	return -1;
    }

    for (int i = 1; i < argc; i++){
	if (strcmp(argv[i], "-l") == 0){
            drop_prob = atof(argv[++i]);
	    cout << "server: loss rate is " << drop_prob << endl;
        }
        if (strcmp(argv[i], "-t") == 0){
            timeout = atoi(argv[++i]);
	    cout << "server: timeout is " << timeout << "us" << endl;
        }
    }


    if (signal(SIGALRM, (void (*) (int)) retransmit) == SIG_ERR) {
	perror("Unable to catch SIGALRM");
	exit(1);
    }

    //Create and bind the TCP socket, let the OS choose the port number
    tcp_socket = socket(AF_INET, SOCK_STREAM,0);
    if(tcp_socket< 0 ){
	cerr << "server: failed to create TCP socket. Exiting." << endl;
	return -1;
    }
	   
    memset(&server_tcp_addr, 0x00, sizeof(struct sockaddr_in));
    server_tcp_addr.sin_family = AF_INET;
    server_tcp_addr.sin_port = 0;  
    server_tcp_addr.sin_addr.s_addr = INADDR_ANY;
	    
    if(bind(tcp_socket, (sockaddr*)&server_tcp_addr, slen) < 0){
        cerr << "server: failed to bind TCP socket. Exiting." << endl;
	return -1;
    }	   

    // get the port number of the socket from the OS and print it out; 
    // it will be a command-line parameter to the base program
    getsockname(tcp_socket, (sockaddr*)&server_tcp_addr, &slen);
    cout << "server: TCP connection port number is: " << server_tcp_addr.sin_port <<endl;

	
    // configure the TCP socket (listen) and accept a connection request from the base
    listen(tcp_socket, 1);
    int client_tcp_socket = accept(tcp_socket, (sockaddr*)&client_tcp_addr, &slen);
    if(client_tcp_socket<0){
        cerr << "server: failed to accept TCP connection. Exiting." << endl;
        close(tcp_socket);
        return -1;
    }
    else{
        cout<< "server: successfully connected with base."<<endl;
    }


    // read the control message from the TCP socket
    int recv_bytes = recv(client_tcp_socket, &ctrl_msg, sizeof(UDP_Cntr_MSG), 0);
    if (recv_bytes <= 0){
	cerr <<"server: error in receving control message from base. Exiting."<<endl;
	close(tcp_socket);
        return -1;
    }

    cout <<"server received control parameters.  port: "<< ctrl_msg.port << ", filename: "<< ctrl_msg.filename <<
            ", buf_size: "<< ctrl_msg.bufsize<<endl;  
    cout <<"extracted base IP address: "<< inet_ntoa(client_tcp_addr.sin_addr)<<endl;
	
    // create a UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket< 0 ){
	cerr << "server: failed to create UDP socket. Exiting." << endl;
	return -1;
    }

    // configure a sockaddr_in data structure for sending to the base; note that the IP address field (sin_addr)
    // can be obtained from information returned from the sockaddr data structure returned by the earlier accept() call
    client_udp_addr.sin_family = AF_INET;
    client_udp_addr.sin_port = ctrl_msg.port;
    client_udp_addr.sin_addr = client_tcp_addr.sin_addr;
  
    tval.it_value.tv_sec = 0;

    ifstream ifs(ctrl_msg.filename, ios::in | ios::binary);
    int buf_size = ctrl_msg.bufsize;
    msg.datasize = buf_size + 12;
    
    int i = 0;
    while(true){
        
        //read file
	msg.sequence = i;
        ifs.read(msg.data, buf_size);
        msg.datasize = ifs.gcount();
        
        //set end flag if end of file
        if(ifs.eof()){
            msg.flags = EOFFLAG;
        }
        
        //transmit
        gettimeofday(&now,NULL);
        cout<< endl << msg.sequence <<" is being first transmitted at time ("<< now.tv_sec%100 << "sec, "<< now.tv_usec << "usec).";
        if ( transmit(udp_socket, &msg, sizeof(msg), 0, (sockaddr*)&client_udp_addr, slen) <0 ){
            cerr <<" error in sending ctrl msg to client"<<endl;
            exit(1);
      	}

	// have signal handler receive acks and transmit if necessary
        transcount = 1;
        not_acked = true;

        //retransmit if not acked
	while (not_acked){
            usleep(timeout);
            tval.it_value.tv_usec = timeout;
            cout << "setting timer for ack" << endl;
            if (setitimer(ITIMER_REAL, &tval, NULL) == -1) {
                perror("error calling setitimer()");
                exit(1);
            }
            pause();
	}
        
        //break if needed
        if (finish == true && not_acked == false){
            break;
        }
        
        if(ifs.eof() && not_acked == false){
            break;
        }
        
        i++;

    }
    
    if(true){
        
        //send the FINFLAG
        end_msg.sequence = -1;
        end_msg.flags = FINFLAG;
        transcount = 1;
        bool end_not_acked = true;
        cout << "\tTransmit last packet (with finish flag): "<< end_cnt << " times" << endl;
        if ( transmit(udp_socket, &end_msg, sizeof(end_msg), 0, (sockaddr*)&client_udp_addr, slen) <0 ){
            cerr <<"\terror in sending ctrl msg to client"<<endl;
            exit(1);
        }
        
        //resend if needed
        while (end_not_acked){
            end_cnt ++;
            usleep(1000);
            tval.it_value.tv_usec = timeout;
            cout << "setting timer for ack" << endl;
            cout << "\tRetransmit last packet (with finish flag): "<< end_cnt << " times" << endl;
            if (setitimer(ITIMER_REAL, &tval, NULL) == -1) {
                perror("error calling setitimer()");
                exit(1);
            }
            pause();
            if (finish == true){
                break;
            }
            if(end_cnt >= 10){
                break;
            }
        }
    }
    
    // close sockets
    close(udp_socket);
    close(tcp_socket);
}


void retransmit(void) {

    // see if we have an ack from the base station
    // use recv with MSG_PEEK flag
    // need a loop since could be more than one ack?
    
    struct timeval now;

    cout << "\tentering retransmit " << endl;

    while (recvfrom(udp_socket, &ack_msg,sizeof(ack_msg), MSG_DONTWAIT, 0, 0) > 0){
	cout << "Received ACK of " << ack_msg.sequence << endl;

	// only done if it is ack for the current packet
	if (ack_msg.sequence == msg.sequence)
            not_acked = false;
        if (ack_msg.sequence == end_sequence)
            finish = true;
    }

    if (not_acked){
	// retransmit the packet
	gettimeofday(&now,NULL);
        cout<< "Retransmit " << msg.sequence <<" at ("<< now.tv_sec%100 << "sec, "<< now.tv_usec << "usec);";

	if ( transmit(udp_socket, &msg, sizeof(msg), 0, (sockaddr*)&client_udp_addr, slen) <0 ){
		cerr <<"\terror in sending ctrl msg to client"<<endl;
		exit(1);
  	}
	transcount++;
    cout << "leaving retransmit, transcount = "<< transcount << endl;
    }
    
    //cases for the last packet
    else if(!finish && end_msg.sequence == end_sequence){
        // retransmit the packet
	gettimeofday(&now,NULL);
        cout<< "Retransmit " << end_msg.sequence <<" at ("<< now.tv_sec%100 << "sec, "<< now.tv_usec << "usec);";

	if ( transmit(udp_socket, &end_msg, sizeof(end_msg), 0, (sockaddr*)&client_udp_addr, slen) <0 ){
		cerr <<"\terror in sending ctrl msg to client"<<endl;
		exit(1);
  	}
	transcount++;
    cout << "leaving retransmit, transcount = "<< transcount << endl;
    }
}
