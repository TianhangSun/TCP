/*
 *  message.h
 */

#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#define MAXNAME 64
#define MAXDATA 10000
#define DATAFLAG 0x0
#define EOFFLAG 0x1
#define FINFLAG 0x2

typedef struct UDP_Control_MSG_Type
{
  int port;
  int bufsize;
  char filename[MAXNAME];  // in microseconds
  
}UDP_Cntr_MSG;

typedef struct Ack_MSG_Type
{
  int sequence;

}Ack_MSG;

typedef struct Data_MSG_Type
{
  int sequence;
  int datasize;
  int flags;
  char data[MAXDATA];
}Data_MSG;


#endif
