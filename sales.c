#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "wrappers.h"
#include "shmem.h"

// signal handling: SIGINT, SIGTERM
//  kill all children
//  destroy all ipc

void cleanup();
void sigHandle(int);

sem_t *factory_mutex, *shm_mutex, *factories_done, *print_report;
key_t mail_key, mem_key;
int mail_id, mem_id;
shData *data;

void cleanup() {
    Shmdt( data );
    shmctl( mem_id, IPC_RMID, NULL );

    msgctl( mail_id, IPC_RMID, NULL );

    Sem_close( factory_mutex );  Sem_unlink( "leachjr_factory.log" );
    Sem_close( shm_mutex );      Sem_unlink( "leachjr_shm_mutex" );
    Sem_close( factories_done ); Sem_unlink( "leachjr_factories_done" );
    Sem_close( print_report );   Sem_unlink( "leachjr_print_report" );
}

void sigHandle (int sig) {
    cleanup();
    kill( 0, SIGKILL );
}

int main (int argc, char** argv) {

    // number of factory lines to make
    if ( argc < 3 ) {
        printf( "there must be at least 2 command lines arguments\n" );
        exit( -1 );
    }
    
    int n    = strtol( argv[1], NULL, 10 );
    int size = strtol( argv[2], NULL, 10 );

    if (n > 40) {
        printf( "there may not be more than 40 factories.\n" );
        exit( -1 );
    }

    sigactionWrapper( SIGINT, sigHandle );
    sigactionWrapper( SIGTERM, sigHandle );

    printf( "SALES: Will Request an Order of Size = %d parts\n", size );

    int fd;

    mail_key = ftok( "message.h", 0 );
    mail_id = Msgget( mail_key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    mem_key  = ftok( "shmem.h",   0 );
    mem_id  = Shmget( mem_key, SHMEM_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR );
    data = (shData*) Shmat( mem_id, NULL, 0 );


    data -> order_size = size;
    data -> made       = 0;
    data -> remain     = size;

    // Set up synchronization mechanisms
    // Creates message queue for factories to communicate with supervisor

    fd = open( "factory.log", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );

    factory_mutex  = Sem_open( "leachjr_factory.log",    O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1 );
    shm_mutex      = Sem_open( "leachjr_shm_mutex",      O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1 );
    factories_done = Sem_open( "leachjr_factories_done", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0 );
    print_report   = Sem_open( "leachjr_print_report",   O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0 );

    printf( "Creating %d Factory(ies)\n", n );

    srandom( time(NULL) );

    for ( int i = 1; i < n+1; i ++ ) {

        char id[3], capacity[3], duration[5];

        snprintf( id,       3, "%d",  i );
        snprintf( capacity, 3, "%ld", random() % 41  + 10 );
        snprintf( duration, 5, "%ld", random() % 701 + 500);

        if ( Fork() == 0 ) {
            // redirect stdout to mutex protected factory.log
            dup2( fd, STDOUT_FILENO );

            execlp( "./factory", "factory", id, capacity, duration, (char*) NULL );
        }

        printf( 
            "SALES: Factory #%3s was created, with Capacity=%4s and Duration=%4s\n",
            id, capacity, duration
        );

    }

    close( fd );

    fd = open( "supervisor.log", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );

    if ( Fork() == 0 ) {
        
        // redirect stdout to supervisor.log

        dup2( fd, STDOUT_FILENO );

        char numlines[3];

        snprintf( numlines, 3, "%d", n );

        execlp( "./supervisor", "supervisor", numlines, (char*) NULL );
    }

    // Waits on semaphore from supervisor to indicate production is done
    // Posts semaphore to tell supervisor to print report
    Sem_wait( factories_done );
    printf( "SALES: Supervisor says all Factories have completed their mission\n" );

    printf( "SALES: Permission granted to print final report\n" );
    Sem_post( print_report );

    // Wait on all children to be destroyed
    int wstatus = 0;

    for ( int i = 0; i < n + 1; i ++ ) {
        waitpid( -1, &wstatus, 0 );
    }


    // Destroy message queue and shared memory

    printf( "SALES: Cleaning up after the Supervisor Factory Processes\n" );
    
    cleanup();
}
