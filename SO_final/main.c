#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <strings.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_COMMANDS 10
#define INDIM 30
#define OUTDIM 512

#define MAX_INPUT_SIZE 100

int sockfd;
int NumThreads;
socklen_t addrlen;

int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}


void *applyCommands(){
    while (1) {

        struct sockaddr_un client_addr;
        char in_buffer[INDIM],out_buffer[OUTDIM];
        int res;
        int c;

        addrlen=sizeof(struct sockaddr_un);

        c = recvfrom(sockfd, in_buffer, sizeof(in_buffer)-1, 0,(struct sockaddr *)&client_addr, &addrlen); //recebeu algo
        if (c <= 0) continue;
    
        //Preventivo, caso o cliente nao tenha terminado a mensagem em '\0', 
        in_buffer[c]='\0';
        char token, type;
        char other_name[MAX_INPUT_SIZE];
        char name[MAX_INPUT_SIZE];
        sscanf(in_buffer,"%c",&token);

        if (token == 'm') {           
            int numTokens = sscanf(in_buffer, "%c %s %s", &token, name, other_name);/*ler os args do move*/
        
            if (numTokens < 2) { /*Verificar se sao os argumentos certos*/
                fprintf(stderr, "Error: invalid command in Queue\n");
                exit(EXIT_FAILURE);
            }
            printf("Move: %s\n",name);
            move(name,other_name); /*chamar o move*/
            sendto(sockfd, out_buffer, c+1, 0, (struct sockaddr *)&client_addr, addrlen);
        }
    
        else {
            int numTokens = sscanf(in_buffer, "%c %s %c", &token, name, &type);
            if (numTokens < 2) {
                fprintf(stderr, "Error: invalid command in Queue\n");
                exit(EXIT_FAILURE);
            }
            int searchResult;

            switch (token) {
                case 'c':
                    switch (type) {
                        case 'f':
                            printf("Create file: %s\n", name);
                            res = create(name, T_FILE);
                            break;
                        case 'd':
                            printf("Create directory: %s\n", name);
                            res = create(name, T_DIRECTORY);
                            break;
                        default:
                            printf("Error: invalid node type\n");
                            exit(EXIT_FAILURE);
                    }
                    break;
                case 'l':
                    searchResult = lookup(name);
                    if (searchResult >= 0)
                        printf("Search: %s found\n", name);
                    else
                        printf("Search: %s not found\n", name);
                    break;

                case 'd':
                    printf("Delete: %s\n", name);
                    delete(name);
                    break;
                default: { /* error */
                    fprintf(stderr, "Error: command to apply\n");
                    exit(EXIT_FAILURE);
                }
                
            }
            
        }
        c = sprintf(out_buffer, "%d", res);
        printf("%s\n",out_buffer);
        sendto(sockfd, out_buffer, c+1, 0, (struct sockaddr *)&client_addr, addrlen);//PARA AQUI
        printf("manda\n");
    }
    return 0;
}


int main(int argc, char* argv[]) {
    struct sockaddr_un server_addr;
    char *path;

    struct timeval start,end;
    int i;
    
    // Verificacoes iniciais
    if (argc != 3){
        fprintf(stderr,"Not enough arguments\n");
        exit(EXIT_FAILURE);
    }

    NumThreads = atoi(argv[1]);
    printf("numthreads: %d\n",NumThreads);

    if(NumThreads < 1){
        printf("Error in the number of threads\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_t tid[NumThreads];
    
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("server: can't open socket");
        exit(EXIT_FAILURE);
    }

    path = argv[2];

    unlink(path);

    addrlen = setSockAddrUn (argv[2], &server_addr);
    
    if (bind(sockfd, (struct sockaddr *) &server_addr, addrlen) < 0) {
        perror("server: bind error");
        exit(EXIT_FAILURE);
    }

    /* init filesystem */
    init_fs();
    gettimeofday(&start,NULL);
    /* process input and print tree */

    for(i=0;i<NumThreads;i++) { /*Chamar threads para o apply command*/
                        printf("thread\n");

        if(pthread_create(&tid[i],NULL,applyCommands,NULL)!=0){
            exit(EXIT_FAILURE);
        }
    }

    for(i=0; i<NumThreads;i++){
        pthread_join(tid[i],NULL);
        
    }

    //Fechar e apagar o nome do socket, apesar deste programa 
    //nunca chegar a este ponto

    close(sockfd);
    unlink(argv[2]);
    /* release allocated memory */
    destroy_fs();

    gettimeofday(&end,NULL);
    double time = (end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec)/(double)1000000;
    printf("TecnicoFS completed in %.4lf seconds.\n",time);

    exit(EXIT_SUCCESS);
}
