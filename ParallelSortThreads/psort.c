#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>



typedef struct rec {
    int key;
    char* value; // 96 bytes //aayush
} rec_t;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
rec_t* records_array;
int per_thread = 0;
int num_records = 0;
int thread_count = 0;
int num = 0;
//HERE

void merge(rec_t* array, int left, int middle, int right) {
    int i = 0;
    int j = 0;
    int k = 0;
    int left_length = middle - left + 1;
    int right_length = right - middle;
    rec_t *left_array = (rec_t*) malloc(left_length*sizeof(rec_t));
    rec_t *right_array = (rec_t*) malloc(right_length*sizeof(rec_t));

    //printf("LEFT %d MID %d RIGHT %d", left, middle, right);
    /* copy values to left array */
    for (int i = 0; i < left_length; i ++) {
        left_array[i] = array[left + i];
        //printf("LEFT KEY: %d and VALUE: %ls \n", array[left + i].key, array[left + i].value);
    }
    /* copy values to right array */
    for (int j = 0; j < right_length; j ++) {
        right_array[j] = array[middle + 1 + j];
        //printf("RIGHT KEY: %d and VALUE: %ls \n", array[left + i].key, array[left + i].value);
    }

    i = 0;
    j = 0;
    k= left;
    //pthread_mutex_lock(&lock); 
    while (i < left_length && j < right_length) {
        if (left_array[i].key <= right_array[j].key) {
            array[k] = left_array[i];
            i ++;
        } else {
            array[k] = right_array[j];
            j++;
        }
        k ++;
    }

    /* copy the remaining values to the array */
    while (i < left_length) {
        array[k] = left_array[i];
        k ++;
        i ++;
    }
    while (j < right_length) {
        array[k] = right_array[j];
        k ++;
        j ++;
    }
    free(left_array);
    free(right_array);
    //pthread_mutex_unlock(&lock); 
}

void merge_sort(rec_t* array, int left, int right) {
    int middle = left + (right - left) / 2;

    if (left < right) {
        merge_sort(array, left, middle);
        merge_sort(array, middle + 1, right);
        merge(array, left, middle, right);
    }
}


void thread_merge_sort(void* arg)
{
    int thread_id = (long) arg;
    int left = thread_id * (per_thread);
    int right = (thread_id + 1) * (per_thread) - 1;
    // if (thread_id == thread_count - 1) {
    //     right += num_records % thread_count; //+= maybe?
    // } aayush
    int middle = left + (right - left) / 2;
    if (left < right) {
        merge_sort(records_array, left, right);
        merge_sort(records_array, left + 1, right);
        merge(records_array, left, middle, right);
    }
}




int main(int argc, char *argv[]){

    if(argc != 3){
        //error();
    }

    struct stat st;
    
    int fd_in = open(argv[1], O_RDONLY, S_IRUSR | S_IWUSR);  
    int fd_out = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fstat(fd_in, &st) == -1 || fd_out == -1) {
        //error();
    }
    if(st.st_size == 0){
        //error();
    }
    char* file_in;
    if((file_in = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd_in, 0)) == MAP_FAILED){
        //error();
    }
    num_records = ((int)st.st_size) / 100;
    records_array = malloc(num_records * sizeof(rec_t));


    rec_t* temp = records_array;
    for(int i = 0; i < (int)st.st_size; i+=100){
        memcpy(&temp[i/100].key, file_in + i, 4);
        temp[i/100].value = malloc(100*sizeof(char));
        memcpy(temp[i/100].value, file_in + i, 100);
    }

    thread_count = get_nprocs();
    pthread_t threadList[thread_count];

    
    for(int i = 0; i < thread_count; i++){
        threadList[i] = i;
        pthread_create(&threadList[i], NULL, (void*)thread_merge_sort, NULL);
    }
    //printf("Adding these everywhere 8 \n");

    for(int i = 0; i < thread_count; i++){
        pthread_join(threadList[i], NULL);
    }

    //printf("Adding these everywhere 9 \n");


    merge_sort(records_array, 0, num_records - 1);

    temp = records_array;
    for(int i = 0; i < num_records; i++){
        if(write(fd_out, temp[i].value, 100) == -1){
            //error();
        }
    }

    munmap(file_in, st.st_size);
    fsync(fd_in);
    close(fd_in);
    fsync(fd_out);
    close(fd_out);

    free(records_array);
}


// int main(int argc, char *argv[]) {
//     char *input = argv[1];
//     //char *output = argv[2]; aayush

//     if (argc != 3) {
//         exit(1);
//     }

//     int fd = open(input, O_RDONLY, S_IRUSR | S_IWUSR);
//     if(fd < 0){
//         printf("\n\"%s \" could not open\n",
//                input);
//         exit(1);
//     }

//     struct stat info;
//     int err = fstat(fd, &info);
//     if(err < 0){
//         printf("\n\"%s \" could not open\n",
//                input);
//         exit(2);
//     }

//     char *records_in = mmap(NULL,info.st_size,
//                             PROT_READ ,MAP_PRIVATE, //aayush
//                             fd,0);
//     if(records_in == MAP_FAILED){
//         close(fd);
//         printf("Mapping Failed\n");
//         return 1;
//     }


//     thread_count = get_nprocs();
//     num_records = info.st_size/100;
//     per_thread = num_records/thread_count;
//     if (thread_count > num_records) {
//         thread_count = num_records;
//         per_thread = 1;
//     }
//     pthread_t threads[thread_count];
   
//     records_array = (rec_t*) malloc(num_records * sizeof(rec_t));
//     rec_t* pointer = records_array;
//     for (int i=0; i < info.st_size; i+=100) {
        
//         memcpy(&pointer->key, records_in + i, 4);
//         pointer->value = malloc(100*sizeof(char));
//         memcpy(pointer->value, records_in + i, 100);
//         pointer++;
//     }
//     fsync(fd);
//     close(fd);
//     int fd_out = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
//     if (fd_out == -1) {
//         exit(0);
//     }
//     rec_t* temp = records_array;
//     for(int i = 0; i < num_records; i++){
//         if(write(fd_out, temp->value, 100) == -1){
//             exit(0);
//         }
//     }
//     fsync(fd_out);
//     close(fd_out);
//     return -1;

//     for (long i = 0; i < thread_count; i++) {
//         threads[i] = i;
//         pthread_create(&threads[i], NULL, (void*)thread_merge_sort, NULL);
//     }

//     for(long i = 0; i < thread_count; i++) {
//         pthread_join(threads[i], NULL);
//     }

    
//     //memcpy(records_array, records, info.st_size);
//     // if (thread_count > num_records) {
//     //     thread_count = num_records;
//     //     per_thread = 1;
//     // }
//     //merge_sections_of_array(records_array);

//     // for (int i = 0; i < record_index; i++) {
//     //     printf("Key: %d   Value: %ls \n", records_array[i].key, records_array[i].value);
//     // }
//     merge_sort(records_array, 0, num_records - 1);

//     // FILE *fptr = fopen(output, "w");
//     // for (int i = 0; i < record_index; i++) {
//     //     fprintf(fptr,"%s",records_array[i].value);
//     // }
//     int fd_out = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
//     if (fd_out == -1) {
//         exit(0);
//     }
//     rec_t* temp = records_array;
//     for(int i = 0; i < num_records; i++){
//         if(write(fd_out, temp->value, 100) == -1){
//             exit(0);
//         }
//     }

// }
