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

using namespace std;

extern double drop_prob;

ssize_t transmit(int sockfd, const void *buf, size_t len, int flags,
				 const struct sockaddr *dest_addr, socklen_t addrlen)
{
      //  drop message? if greater than drop_prob, do not drop
      if(((double)random()/(double)RAND_MAX) > drop_prob) {
		if ( sendto(sockfd, buf, len, flags, dest_addr, addrlen) < 0 ) {
		  cout << "Sendto failed.  Exiting." << endl;
		  return(-1);
        }
		else {
		  cout << " Success." << endl;
		  return(0);
	    }
	  }  
	  else {
		  cout << " Dropped." << endl;
		  return(0);
      }
}



