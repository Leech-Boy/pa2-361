#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>

#include "wrappers.h"
#include "shmem.h"
#include "message.h"

int main( int argc, char** argv ) {

    if ( argc < 2 ) {
        fprintf( stderr, "supervisor expected one command line argument\n" );
        exit( -1 );
    }

    printf( "\nSUPERVISOR: Started\n" );

    int numlines = strtol( argv[1], NULL, 10 );
    int finished_lines = 0;

    int *parts_produced =  (int*)  malloc( sizeof(int) * (numlines + 1) );
    int *iterations     =  (int*)  malloc( sizeof(int) * (numlines + 1) );

    int reported_made = 0;

    for ( int i = 0; i < numlines + 1; i ++ ) {
        parts_produced[i] = 0;
        iterations[i]     = 0;
    }

    sem_t *factories_done, *print_report;

    key_t mail_key = ftok( "message.h", 0 );
    int mail_id = Msgget( mail_key, 0 );

    key_t mem_key = ftok( "shmem.h", 0 );
    int mem_id = Shmget( mem_key, SHMEM_SIZE, 0 );
    shData* data = (shData*) Shmat( mem_id, NULL, 0 );

    sem_t* shm_mutex = Sem_open2( "leachjr_shm_mutex", 0 );

    factories_done = Sem_open2( "leachjr_factories_done", 0 );
    print_report   = Sem_open2( "leachjr_print_report",   0 );

    msgBuf message;

    while ( finished_lines < numlines ) {

        if(msgrcv( mail_id, &message, MSG_INFO_SIZE, 0, 0 ) == -1) {
            perror( "supervisor.c, message receive failed" );
        }

        if ( message.purpose == COMPLETION_MSG ) {
            finished_lines++;
            printf( 
                "SUPERVISOR: Factory # %2d        COMPLETED its task\n",
                message.facID
            );
        } else if ( message.purpose == PRODUCTION_MSG ) {
            printf( 
                "SUPERVISOR: Factory # %2d produced %4d parts in %4d milliseconds\n",
                message.facID, message.partsMade, message.duration
            );
            
            parts_produced[message.facID] += message.partsMade;
            iterations[message.facID] ++;
            
            reported_made += message.partsMade;
        }

    }

    Sem_post( factories_done );
    printf( "\nSUPERVISOR: Manufacturing is complete. Awaiting permission to print final report\n");

    Sem_wait( print_report );

    Sem_wait(shm_mutex);
    int requested = data -> order_size;
    Sem_post(shm_mutex);

    printf( "\n****** SUPERVISOR: Final Report ******\n" );

    for ( int i = 1; i < numlines + 1; i++ ) {
        printf(
            "Factory # %2d made a total of %4d parts in %5d iterations\n",
            i, parts_produced[i], iterations[i]
        );
    }

    printf( "==============================\n" );
    printf( 
        "Grand total parts made = %5d   vs  order size of %5d\n",
        reported_made, requested
    );
    printf( "\n>>> Supervisor Terminated\n" );

    Sem_close( factories_done );
    Sem_close( print_report );

    Shmdt( data );
}
