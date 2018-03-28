/*
    Test the result stored in "data_output" against a serial implementation.

    -----
    Compiling:
    Include "Lab4_IO.c" to compile. Set the macro "LAB4_EXTEND" defined in the "Lab4_IO.c" file to include the extended functions
    $ gcc serialtester.c Lab4_IO.c -o serialtester -lm 

    -----
    Return values:
    0      result is correct
    1      result is wrong
    2      problem size does not match
    253    no "data_output" file
    254    no "data_input" file
*/
#define LAB4_EXTEND

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "Lab4_IO.h"
#include "timer.h"
#include "mpi.h"


void printArray(double* array, int size, int offset){

    int i;
    for (i = offset; i < size; ++i)
    {
        printf("%f ", array[i] );
    }
    printf("\n");
}

#define EPSILON 0.00001
#define DAMPING_FACTOR 0.85

#define THRESHOLD 0.0001

int main (int argc, char* argv[]){
    struct node *nodehead;
    int nodecount;
    int *num_in_links, *num_out_links;
    double *r, *r_pre;
    int i, j, dest;
    double damp_const;
    int iterationcount = 0;

    int npes, myrank;
    double start, end;
    int local_nodecount;
    int offset;
    int * sendcounts, * displs; 
    int rem;

    MPI_Status status;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &npes);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    if (get_node_stat(&nodecount, &num_in_links, &num_out_links)) return 254;

    if (node_init(&nodehead, num_in_links, num_out_links, 0, nodecount)) return 254;
    
    r = malloc(nodecount * sizeof(double));
    r_pre = malloc(nodecount * sizeof(double));

    rem = nodecount%npes;
    offset = 0;


    sendcounts = malloc(sizeof(int)*npes);
    displs = malloc(sizeof(int)*npes);

    //this figures out the parition size that each process gets and its offset from the beginning of the array
    for (i = 0; i < npes; i++) {
        sendcounts[i] = nodecount/npes;
        if (rem > 0) {
            sendcounts[i]++;
            rem--;
        }

        displs[i] = offset;
        offset += sendcounts[i];
    }

    local_nodecount = sendcounts[myrank];
    offset = displs[myrank];

    for ( i = 0; i < nodecount; ++i)
        r[i] = 1.0 / nodecount;
    damp_const = (1.0 - DAMPING_FACTOR) / nodecount;

   
    GET_TIME(start);
    // CORE CALCULATION
    do{
        ++iterationcount;

        //copy r to r_pre
        vec_cp(r, r_pre, nodecount); 
        
        //master process tasks
        if (myrank == 0){
            // printf("%d started\n", myrank );
            for (dest=1; dest<npes; dest++) {
               
                //send to each process its parition of the array
                MPI_Send(&r[displs[dest]], sendcounts[dest], MPI_DOUBLE, dest, 2, MPI_COMM_WORLD);

            }

            //do the page rank algorithm
            for ( i = 0; i < local_nodecount; ++i){
                r[i] = 0;
                for ( j = 0; j < nodehead[i].num_in_links; ++j)
                    r[i] += r_pre[nodehead[i].inlinks[j]] / num_out_links[nodehead[i].inlinks[j]];
                r[i] *= DAMPING_FACTOR;
                r[i] += damp_const;
            }

            //grab the partitions that each process worked on and build up the master r 
            for (dest=1; dest<npes; dest++) {
                MPI_Recv(&r[displs[dest]], sendcounts[dest], MPI_DOUBLE, dest, 3, MPI_COMM_WORLD, &status);
            }
        }

        //all other processes
        if(myrank > 0){
            
            //get the parition this process is assigned
            MPI_Recv(&r[offset], local_nodecount, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, &status);

            //do the page rank algorithm
            for ( i = offset; i < local_nodecount+offset; ++i){
                r[i] = 0;
                for ( j = 0; j < nodehead[i].num_in_links; ++j)
                    r[i] += r_pre[nodehead[i].inlinks[j]] / num_out_links[nodehead[i].inlinks[j]];
                r[i] *= DAMPING_FACTOR;
                r[i] += damp_const;
            }

            //send the process' completed partition back to master
            MPI_Send(&r[offset], local_nodecount, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);
        }

        //broadcast the complete r array from master to all others
        MPI_Bcast(&r[0], nodecount, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    }while(rel_error(r, r_pre, nodecount) >= EPSILON);
    //printf("Program converges at %d th iteration.\n", iterationcount);



    // post processing
    GET_TIME(end);
    if (myrank == 0){
        Lab4_saveoutput(r, nodecount, end-start);
        printf("elapsed time: %f\n",(end-start) );
    }
    MPI_Finalize();

    node_destroy(nodehead, nodecount);
    free(num_in_links); free(num_out_links);

    return 0;
    
}
