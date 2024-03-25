/*----------------------------------------------------
Assignment  :   PA2-IPC
Date        :   03/25/2024
Authors     :   Josiah Leach    leachjr@dukes.jmu.edu
                Luke Hennessy   henneslk@dukes.jmu.edu
File Name   :   sales.c
----------------------------------------------------*/

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

#define LOG_MUTEX_NAME
#define MEM_MUTEX_NAME
#define FAC_DONE_SEM_NAME
#define PRINT_REPORT_SEM_NAME

void cleanup();
void sigHandle(int);

// Global variables required for cleanup
sem_t *factory_mutex, *shm_mutex, *factories_done, *print_report;
int mail_id, mem_id;
shData *data;

void cleanup() {
    Shmdt( data );
    shmctl( mem_id, IPC_RMID, NULL );

    msgctl( mail_id, IPC_RMID, NULL );

    Sem_close( factory_mutex );  Sem_unlink( LOG_MUTEX_NAME );
    Sem_close( shm_mutex );      Sem_unlink( MEM_MUTEX_NAME );
    Sem_close( factories_done ); Sem_unlink( FAC_DONE_SEM_NAME );
    Sem_close( print_report );   Sem_unlink( PRINT_REPORT_SEM_NAME );
}

void sigHandle (int sig) {
    cleanup();
    kill( 0, SIGKILL );
}

int main (int argc, char** argv) {

    // Validate command lines arguments

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

    printf( "SALES: Will Request an Order of Size = %d parts\n", size );


    // Signal handling
    sigactionWrapper( SIGINT, sigHandle );
    sigactionWrapper( SIGTERM, sigHandle );


    // IPC initialization

    // message queue
    key_t mail_key = ftok( "message.h", 0 );
    mail_id = Msgget( mail_key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    // shared memory
    key_t mem_key  = ftok( "shmem.h",   0 );
    mem_id  = Shmget( mem_key, SHMEM_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR );
    data = (shData*) Shmat( mem_id, NULL, 0 );
    data -> order_size = size;
    data -> made       = 0;
    data -> remain     = size;

    // mutex semaphores
    factory_mutex  = Sem_open( LOG_MUTEX_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1 );
    shm_mutex      = Sem_open( MEM_MUTEX_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1 );

    // rendezvous semaphores
    factories_done = Sem_open( FAC_DONE_SEM_NAME,       O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0 );
    print_report   = Sem_open( PRINT_REPORT_SEM_NAME,   O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0 );


    // prepare to make factories

    printf( "Creating %d Factory(ies)\n", n );

    int factory_fd = open( "factory.log", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );

    srandom( time(NULL) );


    // makes factories.
    // IMPORTANT: i starts at 1 because factory id's start at 1.
    for ( int i = 1; i < n+1; i ++ ) {

        char id[3], capacity[3], duration[5];

        // puts command line arguments into string buffers
        // IMPORTANT: modulus operands are 41 and 701, because the range must be inclusive.
        snprintf( id,       3, "%d",  i );
        snprintf( capacity, 3, "%ld", random() % 41  + 10 );
        snprintf( duration, 5, "%ld", random() % 701 + 500);

        if ( Fork() == 0 ) {
            // redirect stdout to mutex protected factory.log
            dup2( factory_fd, STDOUT_FILENO );

            if ( execlp( "./factory", "factory", id, capacity, duration, (char*) NULL ) == -1 ) {
                perror("factory exec failed");
                return -1;
            }
        }

        printf( 
            "SALES: Factory #%3s was created, with Capacity=%4s and Duration=%4s\n",
            id, capacity, duration
        );

    }


    // make supervisor process

    if ( Fork() == 0 ) {
        
        // redirect stdout to supervisor.log
        int supervisor_fd = open( "supervisor.log", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );
        dup2( supervisor_fd, STDOUT_FILENO );

        // put parameter in a string buffer
        char numlines[3];
        snprintf( numlines, 3, "%d", n );

        if ( execlp( "./supervisor", "supervisor", numlines, (char*) NULL ) == -1 ) {
            perror("exec supervisor failed");
            return -1;
        }
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


    // Destroy IPC

    printf( "SALES: Cleaning up after the Supervisor Factory Processes\n" );
    
    cleanup();
}
