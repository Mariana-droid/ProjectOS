#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

char SOCKET_CLIENT[15];
int pid_client; 
int sockfd;
socklen_t servlen, clilen;
struct sockaddr_un serv_addr, client_addr;
char buffer[10];

int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}


int tfsCreate(char *filename, char nodeType) {
  char command[100];
  if (nodeType == 'f') {
    snprintf (command, 100, "%c %s %c",'c',filename,nodeType);
    if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
      perror("client: sendto error");
      return -1;
    }
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, 0, 0) < 0) {
      perror("client: recvfrom error");
      return -1;
    }
    return 0;
    
  }
  else if (nodeType == 'd'){
    snprintf (command, 100, "%c %s %c",'c',filename,nodeType);
    if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
      perror("client: sendto error");
      return -1;
    }
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &servlen) < 0) {
      perror("client: recvfrom error");
      return -1;
    }
    return 0;
    
  }
  
  return -1;
}

int tfsDelete(char *path) {
  char command[100];
  snprintf (command, 100, "%c %s",'d',path);
  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
      perror("client: sendto error");
      return -1;
    }
  if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &servlen) < 0) {
      perror("client: recvfrom error");
      return -1;
    }
  return 0;
}

int tfsMove(char *from, char *to) {
  char command[100];
  snprintf (command, 100, "%c %s %s",'m',from,to);
  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
      perror("client: sendto error");
      return -1;
    }
  if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &servlen) < 0) {
      perror("client: recvfrom error");
      return -1;
    }
  return 0;
}

int tfsLookup(char *path) {
  char command[100];
  snprintf (command, 100, "%c %s",'l',path);
  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
      perror("client: sendto error");
      return -1;
    }
  if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &servlen) < 0) {
      perror("client: recvfrom error");
      return -1;
    }
  return 0;
}

int tfsPrintTree(char *filename) {
  char command[100];
  snprintf (command, 100, "%c %s",'p',filename);
  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
      perror("client: sendto error");
      return -1;
    }
  if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &servlen) < 0) {
      perror("client: recvfrom error");
      return -1;
    }
  return 0;
}

int tfsMount(char * sockPath) {

  pid_client = getpid();
  snprintf (SOCKET_CLIENT, 15,"%d",pid_client);
  if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0) ) < 0) {
    perror("client: can't open socket");
    return -1;
  }
  unlink(SOCKET_CLIENT);
  clilen = setSockAddrUn (SOCKET_CLIENT, &client_addr);
  if (bind(sockfd, (struct sockaddr *) &client_addr, clilen) < 0) {
    perror("client: bind error");
    return -1;
  }  
  servlen = setSockAddrUn(sockPath, &serv_addr);
  
  return 0;
}

int tfsUnmount() {

  close(sockfd);

  unlink(SOCKET_CLIENT);
  
  return 0;
}
