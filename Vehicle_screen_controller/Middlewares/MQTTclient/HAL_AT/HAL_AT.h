#ifndef _HAL_AT_H_
#define _HAL_AT_H_

void HAL_AT_SENDcmd(const unsigned char *cmd,int len,int timeout);
void HAL_AT_Recv(char *c,unsigned int timeout);

#endif
