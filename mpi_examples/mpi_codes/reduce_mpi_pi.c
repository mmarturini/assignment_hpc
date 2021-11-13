/*
 * Copyright (C) 2015 - 2016 Master in High Performance Computing
 *
 * Adapted from the net by  Giuseppe Brandino. 
 * Last modified by Alberto Sartori. 
 * Addedd time and promoted to long long all important variables
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <mpi.h>
#define USE MPI
#define SEED 35791246

int main ( int argc , char *argv[ ] )
{
  // coordinates
  double x, y ;

  // number of points inside the circle
  long long int M, local_M ; 
  double pi ;
   
  // times 
  double start_time, end_time;   
  int myid , numprocs , proc ;
  MPI_Status status;
  MPI_Request request;
  // master process
  int master = 0;
  int tag = 123;

  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&myid);

  if ( argc <=1) {
    fprintf (stderr , " Usage : mpi -np n %s number_of_iterations \n", argv[0] ) ;
    MPI_Finalize() ;
    exit(-1) ;
  }

  long long int N = atoll(argv[1])/numprocs;
// take time of processors after initial I/O operation
  start_time = MPI_Wtime();

  // initialize random numbers 
  srand48(SEED*(myid+1)) ; // seed the number generator
  local_M=0;
  long long int i;
  for (i=0; i<N ; i++) {
    // take a point P(x,y) inside the unit square
    x = drand48(); 
    y = drand48();
      
    // check if the point P(x,y) is inside the circle
    if ((x*x + y*y)<1)
      local_M++;
  }

  MPI_Reduce(&local_M, &M, 1, MPI_LONG_LONG, MPI_SUM, master, MPI_COMM_WORLD);

  if (myid == 0) {
    pi = 4.0*M/(N*numprocs);
    end_time=MPI_Wtime();
    printf("\n # of trials = %llu, estimate of pi is %1.9f \n", N*numprocs, pi);
    printf(" \n # walltime on master processor: %10.8f \n", end_time - start_time ); 
  }  


  MPI_Finalize() ; // let MPI finish up /

}
