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
    
    int id = strtol( argv[1], NULL, 10 );
    int capacity = strtol( argv[2], NULL, 10 );
    int duration = strtol( argv[3], NULL, 10 );

    key_t mail_key = ftok( "message.h", 0 );
    int mail_id = Msgget( mail_key, S_IRUSR | S_IWUSR );

    key_t key = ftok( "shmem.h", 0 );
    int shm_id = Shmget( key, SHMEM_SIZE, S_IRUSR | S_IWUSR );
    shData* data = (shData*) Shmat( shm_id, NULL, 0 );

    sem_t* mutex     = Sem_open2( "leachjr_factory.log", 0 );
    sem_t* shm_mutex = Sem_open2( "leachjr_shm_mutex", 0 );

    msgBuf message;
    message.mtype     = 1;
    message.facID     = id;
    message.capacity  = capacity;
    message.duration  = duration;

    int batch_size;
    int working = 1;
    int parts_made = 0;
    int iterations = 0;

    Sem_wait(mutex);
    printf( "Factory # %2d: STARTED. My Capacity =%4d, in%5d milliSeconds\n",
        id, capacity, duration );
    Sem_post(mutex);

    while( working ) {
        batch_size = capacity;

        Sem_wait(shm_mutex);
        if( data->remain > 0 ) {
            if( data->remain < capacity ) {
                batch_size = data->remain;
            }
        } else {
            batch_size = 0;
        }
        data->remain -= batch_size;
        Sem_post(shm_mutex);

        if( batch_size == 0 ) {
            working = 0;
        } else {
            Sem_wait(mutex);
            printf( "Factory # %2d: Going to make %5d parts in %4d milliSecs\n",
                id, batch_size, duration);
            fflush(stdout);
            Sem_post(mutex);

            Usleep( duration * 1000 );

            message.purpose   = PRODUCTION_MSG;
            message.partsMade = batch_size;

            if ( msgsnd( mail_id, &message, MSG_INFO_SIZE, 0 ) == -1 ) {
                perror( "factory.c, production message failed to send" );
            }

            parts_made += batch_size;
            iterations ++;
        }

    }

    message.purpose   = COMPLETION_MSG;

    if(msgsnd( mail_id, &message, MSG_INFO_SIZE, 0 ) == -1 ) {
        perror( "factory.c, completion message failed to send" );
    }

    Sem_wait(mutex);
    printf(
        ">>> Factory # %3d: Terminating after making total of %5d parts in %5d iterations\n",
        id, parts_made, iterations
    );
    Sem_post(mutex);

    Sem_close(mutex);
    Shmdt( data );

}
