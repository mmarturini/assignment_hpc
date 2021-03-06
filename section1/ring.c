#include<stdio.h>
#include<stdlib.h>
#include<mpi.h>
#include<unistd.h>
#include<stdbool.h>


int main(int argc, char* argv[]) {

	MPI_Init(&argc, &argv);
	int rank, numprocs, root = 0;
	double start, end;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

	if (numprocs < 2) {
		if ( rank == root) {
			printf("\nERROR: This program is designed to run on at least 2 processes.\n\n");
			MPI_Finalize();
			exit(0);
		}
	}
	
	// Starting time
	if (rank == root) {
		start = MPI_Wtime();
	}

	// 1D RING
	MPI_Comm RING;
	int ndims = 1;
	const int dims[1] = { numprocs };
	const int periods[1] = { true };
	int reorder = false;
	MPI_Cart_create(MPI_COMM_WORLD, ndims, dims, periods, reorder, &RING);

	// Rank and coordinate of each process in RING
	int coords[1];
	int ring_rank;
	MPI_Comm_rank(RING, &ring_rank);
	MPI_Cart_coords(RING, ring_rank, 1, coords);

	// Taking the ranks of the neighbors for each process
	int left, right;
	MPI_Cart_shift(RING, 0, 1, &left, &right);

	// Defining some useful variables
	int msgleft  = ring_rank;
	int msgright = -ring_rank;
	int recvleft=0, recvright=0;  // buffers for receiving
	int s_tagleft, s_tagright, r_tagleft, r_tagright; // tags
	MPI_Status status; 
	int displacement, backw_counter = numprocs-1, direction = 0;

	// Sending messages through the ring
	for (int i=0; i < numprocs; i++) {
		displacement = i;

		// taking the tags for sending and receiving
		MPI_Cart_shift(RING, direction, displacement, &s_tagright, &s_tagleft);
		MPI_Cart_shift(RING, direction, backw_counter, &r_tagright, &r_tagleft);

		MPI_Send(&msgleft, 1, MPI_INT, left, s_tagleft, RING);
		MPI_Recv(&recvright, 1, MPI_INT, right, r_tagright, RING, &status);
		MPI_Send(&msgright, 1, MPI_INT, right, s_tagright, RING);
		MPI_Recv(&recvleft, 1, MPI_INT, left, r_tagleft, RING, &status);

		MPI_Barrier(RING);

		msgright = recvleft - ring_rank;
		msgleft = recvright + ring_rank;

		backw_counter = backw_counter - 1;
	}

	// Synchronizing the processes and taking the end time
	MPI_Barrier(RING);
	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == root) {
		end = MPI_Wtime();
		printf("\nTotal elapsed time: %.3f seconds\n\n", end - start);
	}
	
	// Printing in order
	int time_to_sleep = 1*ring_rank;
	sleep(time_to_sleep);
	printf("I am process %d and I have received %d messages both from left and right. \nThe final left message I received has tag %d and value %d .\nThe final right message I received has tag %d and value %d\n\n", ring_rank, numprocs, r_tagleft, recvleft, r_tagright, recvright);

	MPI_Finalize();
	return 0;
}
