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


void printArray(double* array, int size){

    int i;
    for (i = 0; i < size; ++i)
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
    int i, j;
    double damp_const;
    int iterationcount = 0;

    int npes, myrank;
    double start, end;
    double *local_r;
    int local_nodecount;
    int * local_lowerbound;
    int * sendcounts, * displs; 
    int rem, offset;

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

    // local_nodecount = nodecount/npes;
    local_r = malloc(nodecount * sizeof(double));

    // local_lowerbound = (myrank*local_nodecount);

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


        vec_cp(r, r_pre, nodecount);        
        // printf("pre from %i:\n", myrank );
        // printArray(r_pre, nodecount);

        MPI_Scatterv(r, sendcounts, displs, MPI_DOUBLE, local_r, sendcounts, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        // MPI_Scatter(r, local_nodecount, MPI_DOUBLE, local_r, local_nodecount, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        // printf("local_r:\n");
        // printArray(local_r, nodecount);
        // printf("%d, %d\n",sendcounts[myrank], displs[myrank] );
        for ( i = 0; i < sendcounts[myrank]; ++i){
            local_r[i] = 0;
            for ( j = 0; j < nodehead[i+displs[myrank]].num_in_links; ++j)
                local_r[i] += r_pre[nodehead[i+displs[myrank]].inlinks[j]] / num_out_links[nodehead[i+displs[myrank]].inlinks[j]];
            local_r[i] *= DAMPING_FACTOR;
            local_r[i] += damp_const;
        }

        MPI_Allgatherv(local_r, sendcounts[myrank], MPI_DOUBLE, r, sendcounts, displs, MPI_DOUBLE, MPI_COMM_WORLD);
        // MPI_Allgather(local_r, local_nodecount, MPI_DOUBLE, r, local_nodecount, MPI_DOUBLE, MPI_COMM_WORLD);

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
