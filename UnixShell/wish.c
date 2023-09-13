#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include <stdbool.h>
#include <fcntl.h>


char error_message[30] = "An error has occurred\n";
int size = 128;
char* delim = " \n\t";
char *initialPath = "/bin";
char *pathList[128];

// typedef struct PathList{
//     char* path;
//     struct PathList* next;
// }PathList;


int readLine(char* line, char *args[]){

    char* input;
    int idx = 0;
    input = strtok(line, delim);
    while(input != NULL){
        args[idx] = input;
        idx += 1;
        input = strtok(NULL, delim);

    }

    return idx;
}
int main(int argc, char* argv[]){

    FILE *f = NULL;
    
    if(argc == 1){  //interactive mode. No file 
        f = stdin;
        //write(STDOUT_FILENO, "wish> ", strlen("wish> "));
    } else if(argc == 2){ //batch mode. Read in file, check for concurrency later
        f = fopen(argv[1], "r");
        if(f == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    } else{ //error
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    pathList[0] = initialPath;
    // PathList *currPath = (PathList*)malloc(sizeof(PathList));
    // currPath->path = "/bin";
    // currPath->next = NULL;

    while(1) { //run until EOF, no more input

        size_t buffer = 0;
        char *line = NULL;

        if(argc == 1){
            printf("wish> ");
            fflush(stdout);
            f = stdin;
        }

        if(getline(&line, &buffer, f) == EOF){
            exit(0);
        }
        if (strcmp(line, "\n") == 0) {
				continue;
		}
        line = strtok(line, "\n");

        char* lineArgs [size];

        int numArgs = readLine(line, lineArgs);

        if(numArgs == 0){ //empty line
            continue;
        }

        int redirectionCount = 0;
        int redirectionIndex;
        bool redir = false;
        for(int i = 0; i < numArgs; i++){
            if(strcmp(lineArgs[i], ">") == 0){
                redirectionCount += 1;
                redirectionIndex = i;
                redir = true;
            }
        }
        if(redir){
            if(redirectionCount > 1 || redirectionIndex + 2 != numArgs){
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }


        //printf("How is redirection %d\n", redirectionIndex);

        //printf("How is command line spaced %d\n", numArgs);

        if(strcmp(lineArgs[0], "exit") == 0){ //exit cmd
            if(numArgs > 1){
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            exit(0);
        }
        else if(strcmp(lineArgs[0], "cd") == 0){ //cd cmd

            if(numArgs != 2) {
					write(STDERR_FILENO, error_message, strlen(error_message)); 
                    continue;
            }
            if(chdir(lineArgs[1]) != 0) {
					write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        else if(strcmp(lineArgs[0], "path") == 0){ //path cmd
            if(numArgs < 0){
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            int idx = 0;
            while(pathList[idx] != NULL){
                pathList[idx] = NULL;
                idx += 1;
            }
            idx = 0;
            for(int i = 1; i < numArgs; i++){
                pathList[idx] = lineArgs[i];
                idx += 1;
            }
        } 
        else if(strcmp(lineArgs[0], "if") == 0){ //if cases

        }
        else{ //searching path
            //printf("Before seg fault %s\n", pathList[0]);

            int idx = 0;
            while(pathList[idx] != NULL){
                char* currPath = strdup(pathList[idx]);
                //printf("CurrPath %s\n", currPath);
                strcat(currPath, "/");
                //printf("lineArgs %s\n", lineArgs[0]);
                strcat(currPath, lineArgs[0]);
                //printf("CurrPathAgain %s\n", currPath);
                lineArgs[numArgs] = NULL;

                if(access(currPath, X_OK) == 0){
                    int rc = fork();
                    if(rc < 0){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        exit(1);
                    }
                    else if (rc == 0 && redirectionCount == 1){
                        int fd = open(lineArgs[numArgs-1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        dup2(fd, 1);   
                        dup2(fd, 2);                                    
                        close(fd);
                        execv(currPath, lineArgs);
                    } else if(rc == 0){
                        execv(currPath, lineArgs);

                    }
                    
                }
                free(currPath);
                //printf("EndOFLoop %s\n", currPath);
                idx += 1;
            }
        }



    }

    return 0;
}