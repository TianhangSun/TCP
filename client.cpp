#include <string.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <netdb.h>
#include <fstream>
#include "message.h"


using namespace std;

ssize_t transmit(int sockfd, const void *buf, size_t len, int flags,
				 const struct sockaddr *dest_addr, socklen_t addrlen);
double drop_prob = 0;  // chance that a packet is dropped

int main(int argc, char *argv[])
{
    //pkm: need to get sensor (server) host and port number from command line; could also user C++ strings instead of a char array
    socklen_t slen = sizeof(sockaddr_in);
    char server_host[20];
    char destination[20];
    int buf_size;
    int tcp_port;
    UDP_Cntr_MSG ctrl_msg;

    // get arguments from the command line (remote host, remote port, num samples, interval)
    if ( (argc != 13)){
        cout << "Usage: " << argv[0];
	cout << " -h <remote-host> -s <src-filename> -d <dst-filename> -b <bufsize> -l <loss-rate> -p <remote-port>" << endl;
        return -1;
    }

    for (int i = 1; i < argc; i++){
	if (strcmp(argv[i], "-h") == 0)
            strcpy (server_host, argv[++i]);
		
	if (strcmp(argv[i], "-p") == 0)
            tcp_port = atoi(argv[++i]);
        
        if (strcmp(argv[i], "-b") == 0){
            buf_size = atoi(argv[++i]);
            ctrl_msg.bufsize = buf_size;
        }
		
	if (strcmp(argv[i], "-s") == 0){
            strcpy(ctrl_msg.filename,argv[++i]);
        }
	
	if (strcmp(argv[i], "-d") == 0)
            strcpy(destination,argv[++i]);

	if (strcmp(argv[i], "-l") == 0)
            drop_prob = atof(argv[++i]);
    }

    cout <<"client is running ...\n";

    cout  << "remote host: " << server_host <<", remote TCP port: "<< tcp_port << ", source: " << ctrl_msg.filename << ", destination: "<< destination
            << ", buffer size: " << buf_size << ", loss rate:" << drop_prob<<endl;

    // create TCP socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM,0);
    if(tcp_socket< 0 ){
	cerr << "client: failed to create TCP socket. Exiting." << endl;
	return -1;
    }

    // get IP address of sensor host via DNS
    hostent *hostp = gethostbyname(server_host);

    // connect to the sensor program
    sockaddr_in server_tcp_add;
    memset(&server_tcp_add, 0x00, sizeof(struct sockaddr_in));
    server_tcp_add.sin_family = AF_INET;
    server_tcp_add.sin_port = tcp_port;
    memcpy(&server_tcp_add.sin_addr, hostp->h_addr, hostp->h_length);

    if( connect(tcp_socket, (struct sockaddr *)&server_tcp_add, sizeof(server_tcp_add)) < 0){
        cerr<< "client: connect() error. Exiting"<<endl;
	close(tcp_socket);
	return -1;
    }

    // create and bind UDP socket, let OS choose the port number
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket< 0 ){
	cerr << "client: failed to create UDP socket. Exiting." << endl;
	return -1;
    }
	
    sockaddr_in server_udp_add;
    server_udp_add.sin_family = AF_INET;
    server_udp_add.sin_port = 0; //pkm: let OS choose port
    server_udp_add.sin_addr.s_addr = htonl(INADDR_ANY);
    if( bind(udp_socket , (struct sockaddr*)&server_udp_add, sizeof(server_udp_add) ) == -1){
        cerr <<" error in bing UDP socket"<<endl;
    }

    // get the port number assigned by the OS
    getsockname(udp_socket, (sockaddr*)&server_udp_add, &slen);
    cout << "client UDP port is " << server_udp_add.sin_port <<endl;
    ctrl_msg.port = server_udp_add.sin_port;

    // send the control message to the server
    int num_bytes = send(tcp_socket, &ctrl_msg, sizeof(ctrl_msg), 0);	
    if (num_bytes == -1){
	cerr <<"client: error in sending control message. Exiting."<<endl;
	close(tcp_socket);
	return -1; 
    }

    // loop, receiving a message from the sensor and transmitting an acknowledgment
    Data_MSG msg; 
    msg.datasize = 100;
    Ack_MSG ack_msg; 
    bool notdone = true;
    ofstream out(destination, std::ofstream::binary);

    //check receive packet
    int last = -1;
    while (notdone){
	if(recvfrom(udp_socket, &msg, sizeof(msg), 0, (sockaddr*)&server_udp_add, &slen) > 0){
            cout<<msg.sequence<<" received." <<endl;
            ack_msg.sequence = msg.sequence;
            if(last != msg.sequence){
                out.write(msg.data, msg.datasize);
                cout << "\tWriting packet: " << msg.sequence << endl;
            }
            last = msg.sequence;
            cout<<"\tTransmitting ACK of " <<  ack_msg.sequence << "." ; 

            if ( transmit(udp_socket, &ack_msg, sizeof(ack_msg), 0, (sockaddr*)&server_udp_add, slen) <0 ){
                cerr <<" error in sending ack to server"<<endl;
                exit(1);
            }
	}
	else{
            cerr <<" error in recvfrom(); exiting."<<endl;			
            exit(1);
	}
        
        //close if end of file
        if(msg.flags == EOFFLAG){
            out.close();
            cout << "\tReceived end of file flag" << endl;
        }
        
        //if FINFLAG, send ack 5 times
        if(msg.flags == FINFLAG){
            notdone = false;
            cout << "\tReceived finish flag, exit" << endl;
            for(int i=0; i<5; i++){
                if ( transmit(udp_socket, &ack_msg, sizeof(ack_msg), 0, (sockaddr*)&server_udp_add, slen) <0 ){
                    cerr <<" error in sending ack to server"<<endl;
                    exit(1);
                }
                cout << " Send the ack of FINFLAG " << i+1 << " times" <<endl;                
            }
        }
    }
    close (tcp_socket); 
    close(udp_socket);
}
