#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "fs/operations.h"
#include <sys/time.h>
#include <pthread.h>

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

int numberThreads = 0;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int buffer_length = 0;
int read_index = 0;
int write_index = 0;
FILE *input;
int NumThreads;
pthread_cond_t cheia,noCommand = PTHREAD_COND_INITIALIZER;
int fim_do_input = 0; /* Ainda faltam comandos -> 0. Ja foram todos os comandos -> 1*/

pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;

void lock_mutex(){
    if (pthread_mutex_lock(&trinco)!= 0){
        exit(EXIT_FAILURE);
    }
}

void unlock_mutex(){
    if (pthread_mutex_unlock(&trinco) != 0){
        exit(EXIT_FAILURE);
    }
}


int insertCommand(char* data) {
    lock_mutex();
    while (buffer_length == MAX_COMMANDS){ 
        pthread_cond_wait(&cheia,&trinco);    
    }
    strcpy(inputCommands[write_index],data);
    /*printf("o que foi enfiado no insert %s\n",data);*/
    write_index++;
    if (write_index == MAX_COMMANDS){
        write_index = 0;
    }
    buffer_length++;
    pthread_cond_signal(&noCommand);
    unlock_mutex();
    return 1;
    /*nao esquecer ERRO (0)!!!!*/
}

const char* removeCommand() {
    lock_mutex();
    const char* buffer;
    while (buffer_length == 0 && fim_do_input==0){
        pthread_cond_wait(&noCommand,&trinco);
        if (buffer_length <= 0 && fim_do_input!=0)
        {   
            unlock_mutex();
            return NULL;
        }
    }
    buffer = inputCommands[read_index];
    read_index++;
    if (read_index == MAX_COMMANDS){
        read_index = 0;
    }
    buffer_length--;
    pthread_cond_signal(&cheia);
    unlock_mutex();
    return buffer;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(){
    char line[MAX_INPUT_SIZE];
    
    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), input)) {
        char token, type;
        char name[MAX_INPUT_SIZE]/*,other_name[MAX_INPUT_SIZE]*/;
        /*sscanf(line, "%c %s %c", &token);
        if (token == 'm')
        {
            int numTokens = sscanf(line, "%c %s %c", &token, name, other_name);
            if (numTokens != 3)
                    errorParse();
            if (! (insertCommand(line)))
                errorParse();
        }
        else
        {*/
            int numTokens = sscanf(line, "%c %s %c", &token, name, &type);
        /* perform minimal validation */
            if (numTokens < 1) {
                continue;
            }
            switch (token) {
                case 'c':
                    if(numTokens != 3){
                        errorParse();
                    }
                    if(insertCommand(line))
                        break;

                case 'l':
                    if(numTokens != 2)
                        errorParse();
                    if(insertCommand(line))
                        break;
            
                case 'd':
                    if(numTokens != 2)
                        errorParse();
                    if(insertCommand(line))
                        break;
                case '#':
                    break;
            
                default: { /* error */
                    errorParse();
                }
            }
        /*}*/
        

    }
    fim_do_input = 1;
    pthread_cond_broadcast(&noCommand);
}

void *applyCommands(){
    while (fim_do_input == 0 || buffer_length > 0){
        const char* command = removeCommand();
            if (command == NULL){
                return 0;
            }
            char token, type;
            /*char other_name[MAX_INPUT_SIZE];*/
            char name[MAX_INPUT_SIZE];
            /*sscanf(command,"%c",&token);
            if (token == 'm')
            {           
                int numTokens = sscanf(command, "%c %s %s", &token, name, other_name);
                printf("Move: %s\n",name);
                move(name,other_name);
            }
            else
            {*/
                int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
                if (numTokens < 2) {
                    fprintf(stderr, "Error: invalid command in Queue\n");
                    exit(EXIT_FAILURE);
                }
            //printf("%c %s %c aqui estao eles\n", token, name, type);
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

            
            
            /*}*/
    }

    return 0;
    
}


int main(int argc, char* argv[]) {
    struct timeval start,end;
    int i;
    if (argc != 4){
        fprintf(stderr,"Not enough arguments\n");
        exit(EXIT_FAILURE);
    }
    input = fopen(argv[1],"r");
    FILE *output = fopen(argv[2],"w");
    NumThreads = atoi(argv[3]);

    if(input == NULL){
        perror("Error opening input file\n");
        exit(EXIT_FAILURE);
    }

    if(output == NULL){
        perror("Error opening output file\n");
        exit(EXIT_FAILURE);
    }

    if(NumThreads < 1){
        printf("Error in the number of threads\n");
        exit(EXIT_FAILURE);
    }
    pthread_t tid[NumThreads];
    
    /* init filesystem */
    init_fs();
    gettimeofday(&start,NULL);
    /* process input and print tree */

    for(i=0;i<NumThreads;i++){
        if(pthread_create(&tid[i],NULL,applyCommands,NULL)!=0){
            exit(EXIT_FAILURE);
        }
    }

    processInput();

    for(i=0; i<NumThreads;i++){
        pthread_join(tid[i],NULL);
        
    }
    pthread_cond_destroy(&cheia);
    pthread_cond_destroy(&noCommand);
    fclose(input);
    
    
    print_tecnicofs_tree(output);
    fclose(output);
    /* release allocated memory */
    destroy_fs();

    gettimeofday(&end,NULL);
    double time = (end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec)/(double)1000000;
    printf("TecnicoFS completed in %.4lf seconds.\n",time);

    exit(EXIT_SUCCESS);
}
