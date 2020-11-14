#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <pthread.h>

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

int Numthreads = 0;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;


/* criacao e inicializacao do mutex */
pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;

pthread_rwlock_t lock;

/* 4 argumento, synchstrategy*/
char tipo[100];


/* lock do mutex, exit failure se o lock der erro */

void lock_mutex(){
    if (pthread_mutex_lock(&trinco)!= 0){
        exit(EXIT_FAILURE);
    }
}

/* unlock do mutex, exit failure se o unlock der erro */

void unlock_mutex(){
    if (pthread_mutex_unlock(&trinco) != 0){
        exit(EXIT_FAILURE);
    }
}

int insertCommand(char* data) {
    lock_mutex();
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        unlock_mutex();
        return 1;
    }
    unlock_mutex();
    return 0;
}



/* adicao do mutex */
char* removeCommand() {
    lock_mutex();
    if(numberCommands > 0){
        numberCommands--;
        unlock_mutex();
        return inputCommands[headQueue++];  
    }
    unlock_mutex();
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(FILE *input){
    char line[MAX_INPUT_SIZE];

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), input)) {
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
}

void *applyCommands(){
    while (numberCommands > 0){
    const char* command = removeCommand();
        if (command == NULL){
            return 0;
        }
        char token, type;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
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
                        create(name, T_FILE);
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        break;
                    default:
                        printf("Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                searchResult = lookup_lock(name);
                lookup_unlock(name);
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
    return 0;
}

int main(int argc, char* argv[]) {
    struct timeval start,end;
    int i;
    
    /*guarda a hora a que comeca */
    gettimeofday(&start,NULL);

    /* erros possiveis */
    if (argc != 4){
        fprintf(stderr,"Not enough arguments");
        exit(EXIT_FAILURE);
    }
    FILE *input = fopen(argv[1],"r");
    FILE *output = fopen(argv[2],"w");
    int Numthreads = atoi(argv[3]);
    pthread_t tid[Numthreads];

    if(input == NULL){
        perror("Error opening input file\n");
        exit(EXIT_FAILURE);
    }

    if(output == NULL){
        perror("Error opening output file\n");
        exit(EXIT_FAILURE);
    }

    if(Numthreads < 1){
        printf("Error in the number of threads\n");
        exit(EXIT_FAILURE);
    }
    
    /* init filesystem */
    init_fs();

    /* process input and print tree */
    processInput(input);

    /* verificacao se os locks sao validos */

        for(i=0;i<Numthreads;i++){
            if(pthread_create(&tid[i],NULL,applyCommands,NULL)){
                exit(EXIT_FAILURE);
            }
        }
        /* espera para que as pthreads acabem pela ordem que foram criadas */
        for(i=0; i<Numthreads;i++){
            pthread_join(tid[i],NULL);
        }   

    print_tecnicofs_tree(output);

    /* release allocated memory */
    destroy_fs();

    /* guarda a hora a que o programa acaba */
    gettimeofday(&end,NULL);

    /* devolve em segundos na variavel time o tempo que o programa demorou */
    double time = (end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec)/(double)1000000;
    printf("TecnicoFS completed in %.4lf seconds.\n",time);

    exit(EXIT_SUCCESS);
}
