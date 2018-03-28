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

    // Calculate the result
    if (node_init(&nodehead, num_in_links, num_out_links, 0, nodecount)) return 254;
    
    r = malloc(nodecount * sizeof(double));
    r_pre = malloc(nodecount * sizeof(double));

    rem = nodecount%npes;
    offset = 0;


    sendcounts = malloc(sizeof(int)*npes);
    displs = malloc(sizeof(int)*npes);

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

    // if (myrank == 0)
    // {   
    //     char processor_name[MPI_MAX_PROCESSOR_NAME];
    //     int name_len;
    //     MPI_Get_processor_name(processor_name, &name_len);
    //     printf("processor %s started\n",  processor_name);
    // }

   
    GET_TIME(start);
    // CORE CALCULATION
    do{
        ++iterationcount;


       
        // printf("pre from %i:\n", myrank );
        // printArray(r_pre, nodecount);
        vec_cp(r, r_pre, nodecount); 

        // MPI_Scatter(r, local_nodecount, MPI_DOUBLE, local_r, local_nodecount, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (myrank == 0){
            // printf("%d started\n", myrank );
            for (dest=1; dest<npes; dest++) {
                // MPI_Send(&r_pre[0], nodecount, MPI_DOUBLE, dest, 1, MPI_COMM_WORLD);
                // printf("%d sent r_pre to %d\n", myrank, dest );
                // MPI_Send(&offset, 1, MPI_INT, dest, 1, MPI_COMM_WORLD);
                MPI_Send(&r[displs[dest]], sendcounts[dest], MPI_DOUBLE, dest, 2, MPI_COMM_WORLD);
                // printf("%d sent r[%d] to %d\n", myrank, offset, dest );
                // printf("Sent %d elements to task %d offset= %d\n",chunksize,dest,offset);
            }

            for ( i = 0; i < local_nodecount; ++i){
                r[i] = 0;
                for ( j = 0; j < nodehead[i].num_in_links; ++j)
                    r[i] += r_pre[nodehead[i].inlinks[j]] / num_out_links[nodehead[i].inlinks[j]];
                r[i] *= DAMPING_FACTOR;
                r[i] += damp_const;
            }

            for (dest=1; dest<npes; dest++) {
                MPI_Recv(&r[displs[dest]], sendcounts[dest], MPI_DOUBLE, dest, 3, MPI_COMM_WORLD, &status);
                // printf("%d recv r from %d\n", myrank, dest );
            }
        }
        if(myrank > 0){
            // printf("%d started\n", myrank);
            // MPI_Recv(&r_pre[0], nodecount, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, &status);
            // printf("%d recv r_pre\n", myrank );
            // MPI_Recv(&offset, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
            MPI_Recv(&r[offset], local_nodecount, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, &status);
            // printf("%d recv r[%d] = %d\n", myrank, offset,r[offset] );
            // printArray(r, nodecount,0);
            for ( i = offset; i < local_nodecount+offset; ++i){
                // printf("%d starting at %d\n",myrank, offset );
                r[i] = 0;
                for ( j = 0; j < nodehead[i].num_in_links; ++j)
                    r[i] += r_pre[nodehead[i].inlinks[j]] / num_out_links[nodehead[i].inlinks[j]];
                r[i] *= DAMPING_FACTOR;
                r[i] += damp_const;
            }
            // printf("r from %d\n", myrank );
            // printArray(r, nodecount, 0);

            MPI_Send(&r[offset], local_nodecount, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);
            // printf("%d sent r[%d]\n", myrank, local_nodecount*myrank );
        }

        MPI_Bcast(&r[0], nodecount, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        // printf("local_r:\n");
        // printArray(local_r, nodecount);


        // MPI_Allgather(local_r, local_nodecount, MPI_DOUBLE, r, local_nodecount, MPI_DOUBLE, MPI_COMM_WORLD);
        // if (myrank == 0)
        // {   
        //     printf("r:\n");
        //     printArray(r, nodecount);
        // }

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
