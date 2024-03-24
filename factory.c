/*-------------------------------------------
Assignment  :   PA2-IPC
Date        :   03/25/2024
Authors     :   Josiah Leach    Luke Hennessy henneslk@dukes.jmu.edu
File Name   :   factory.c
-------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>

#include "wrappers.h"
#include "shmem.h"
#include "message.h"

int main (int argc, char** argv) {
    
    // get ints out of command line string args
    int id       = strtol( argv[1], NULL, 10 );
    int capacity = strtol( argv[2], NULL, 10 );
    int duration = strtol( argv[3], NULL, 10 );


    // access IPC

    // message queue
    key_t mail_key = ftok( "message.h", 0 );
    int mail_id = Msgget( mail_key, S_IRUSR | S_IWUSR );

    // shared memory
    key_t key = ftok( "shmem.h", 0 );
    int shm_id = Shmget( key, SHMEM_SIZE, S_IRUSR | S_IWUSR );
    shData* data = (shData*) Shmat( shm_id, NULL, 0 );

    // mutexes
    sem_t* log_mutex = Sem_open2( "leachjr_factory.log", 0 );
    sem_t* shm_mutex = Sem_open2( "leachjr_shm_mutex", 0 );


    // initialize parts of the message that will never change
    msgBuf message;
    message.mtype     = 1;
    message.facID     = id;
    message.capacity  = capacity;
    message.duration  = duration;


    // initialize data for record keeping
    int parts_made = 0;
    int iterations = 0;


    Sem_wait(log_mutex);
    printf( "Factory # %2d: STARTED. My Capacity =%4d, in%5d milliSeconds\n",
        id, capacity, duration );
    Sem_post(log_mutex);


    // initialize variables for production loop
    int batch_size;
    int working = 1;

    // IMPORTANT: while loop doesn't explicitly check remaining units because
    // that requires semaphore use which would be messy.
    while( working ) {

        // default amount to create of product is capacity.
        batch_size = capacity;


        // all operations with shared memory here.
        Sem_wait(shm_mutex);

        // if the remaining items to produce is less than capacity, make all that remain.
        // This includes the case where there is nothing left to make, which is
        // checked outside of the "critical section"
        if( data->remain < capacity ) {
            batch_size = data->remain;
        }
        data->remain -= batch_size;

        Sem_post(shm_mutex);


        // if the amount that remained to make was 0, exit the loop
        if( batch_size == 0 ) {
            working = 0;
        } else {
            // log production
            Sem_wait(log_mutex);
            printf( "Factory # %2d: Going to make %5d parts in %4d milliSecs\n",
                id, batch_size, duration);
            fflush(stdout);
            Sem_post(log_mutex);

            // produce
            Usleep( duration * 1000 );

            // create production message.
            message.purpose   = PRODUCTION_MSG;
            message.partsMade = batch_size;

            // send production message
            if ( msgsnd( mail_id, &message, MSG_INFO_SIZE, 0 ) == -1 ) {
                perror( "factory.c, production message failed to send" );
            }

            // update production statistics
            parts_made += batch_size;
            iterations ++;
        }

    }


    // create completion message
    message.purpose = COMPLETION_MSG;

    // send completion message
    if(msgsnd( mail_id, &message, MSG_INFO_SIZE, 0 ) == -1 ) {
        perror( "factory.c, completion message failed to send" );
    }


    // log completion
    Sem_wait(log_mutex);
    printf(
        ">>> Factory # %3d: Terminating after making total of %5d parts in %5d iterations\n",
        id, parts_made, iterations
    );
    Sem_post(log_mutex);


    // detach from IPC
    Sem_close(log_mutex);
    Shmdt( data );

}
