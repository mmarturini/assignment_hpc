#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <stdbool.h>


double rand_num() { return (double)rand() / (double)RAND_MAX; }


int main(int argc, char** argv) {

    // Initialization and input checking
    MPI_Init(&argc, &argv);
    int p, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if ( argc != 7 ) {
        if ( rank == 0 ) {
            printf("ERROR: WRONG INPUT ARGUMENTS\n mpiexec -np np ./sum3Dmatrix_with_MPI_Datatype.x nx ny nz npx npy npz\n");
	    printf("Example: mpiexec -np 8 --mca btl ^openib ./sum3Dmatrix_with_MPI_Datatype.x 2400 100 100 2 2 2\n");
        }
        MPI_Finalize();
        exit(0);
    }

    const int NX = atoi(argv[1]);
    const int NY = atoi(argv[2]);
    const int NZ = atoi(argv[3]);
    const int NPX = atoi(argv[4]);
    const int NPY = atoi(argv[5]);
    const int NPZ = atoi(argv[6]);   
    const int BLOCKX = NX / NPX;    // # of elems along x in 3D submatrix blocks 
    const int BLOCKY = NY / NPY;    // # of elems along y in 3D submatrix blocks 
    const int BLOCKZ = NZ / NPZ;    // # of elems along z in 3D submatrix blocks

    if ( NX % NPX != 0 ) {
        if ( rank == 0 )
            printf("ERROR: Number of elements not divisible by number of processes along 1st dimension.\n");
        MPI_Finalize();
        exit(0);
    } 
    else if ( NY % NPY != 0 ) {
        if ( rank == 0 )
            printf("ERROR: Number of elements not divisible by number of processes along 2nd dimension.\n");
        MPI_Finalize();
        exit(0);
    } 
    else if ( NZ % NPZ != 0 ) {
        if ( rank == 0 )
            printf("ERROR: Number of elements not divisible by number of processes along 3rd dimension.\n");
        MPI_Finalize();
        exit(0);
    } 
 
    if ( NPX*NPY*NPZ != p ) {
        if (rank==0) 
            printf("ERROR: Number of processes per dimension inserted does not correspond to total number of processes specified in mpirun -np.\n");
        MPI_Finalize();
        exit(0);
    } 
  
    // Receive buffers for the blocks
    double* recv_buffA = (double*)malloc(sizeof(double) * (BLOCKY * BLOCKX * BLOCKZ));
    double* recv_buffB = (double*)malloc(sizeof(double) * (BLOCKY * BLOCKX * BLOCKZ));

    // Defining user datatype
    MPI_Datatype blocktype;
    MPI_Datatype blocktype2;
    MPI_Type_vector(BLOCKX, BLOCKY * BLOCKZ, NY * NZ, MPI_DOUBLE, &blocktype2);
    MPI_Type_create_resized(blocktype2, 0, sizeof(double), &blocktype);
    MPI_Type_commit(&blocktype);

    // Displacements and counts for MPI_Scatterv and MPI_Gatherv
    int disps[NPX * NPY * NPZ];
    int counts[NPX * NPY * NPZ];
    for (int iii = 0; iii < NPX; iii++) {
        for (int jjj = 0; jjj < NPY; jjj++) {
            for (int kkk = 0; kkk < NPZ; kkk++) {
                disps[iii * NPY * NPZ + jjj*NPZ + kkk] = (iii * NY * NZ * BLOCKX)  +  (jjj * BLOCKY * NZ)  +  kkk*BLOCKZ;
                counts[iii * NPY * NPZ + jjj * NPZ + kkk] = 1;
            }
        }
    }

    // 3D topology
    MPI_Comm TOPOLOGY_3D;
    int topo3d_ndims = 3;
    int topo3d_dims[3] = { NPX, NPY, NPZ };
    const int topo3d_periods[3] = { false, false, false };
    MPI_Dims_create(p, topo3d_ndims, topo3d_dims);
    int reorder = false;
    MPI_Cart_create(MPI_COMM_WORLD, topo3d_ndims, topo3d_dims, topo3d_periods, reorder, &TOPOLOGY_3D);
    int topo3d_coords[3];
    int topo3d_rank;
    MPI_Comm_rank(TOPOLOGY_3D, &topo3d_rank);
    MPI_Cart_coords(TOPOLOGY_3D, topo3d_rank, topo3d_ndims, topo3d_coords);

    // Initialize and scatter 3D matirces A,B
    double start_time;
    if (topo3d_rank == 0) {
	start_time = MPI_Wtime();
        double* A = (double*)malloc(sizeof(double) * NX * NY * NZ);
        double* B = (double*)malloc(sizeof(double) * NX * NY * NZ);
        for (int iii = 0; iii < NX * NY * NZ; iii++) {
            A[iii] = rand_num();
            B[iii] = rand_num();
        }

        MPI_Scatterv(A, counts, disps, blocktype, recv_buffA, BLOCKX * BLOCKY * BLOCKZ, MPI_DOUBLE, 0, TOPOLOGY_3D);
        MPI_Scatterv(B, counts, disps, blocktype, recv_buffB, BLOCKX * BLOCKY * BLOCKZ, MPI_DOUBLE, 0, TOPOLOGY_3D);
    }
    else {
        MPI_Scatterv(NULL, counts, disps, blocktype, recv_buffA, BLOCKX * BLOCKY * BLOCKZ, MPI_DOUBLE, 0, TOPOLOGY_3D);
        MPI_Scatterv(NULL, counts, disps, blocktype, recv_buffB, BLOCKX * BLOCKY * BLOCKZ, MPI_DOUBLE, 0, TOPOLOGY_3D);
    }

    // Performing the sum at block level
    double* block_sums = (double*)malloc(sizeof(double) * (BLOCKY * BLOCKX * BLOCKZ));
    for (int i = 0; i < (BLOCKY * BLOCKX * BLOCKZ); ++i) {
        block_sums[i] = recv_buffA[i] + recv_buffB[i];
    }

    // Gathering the blocks in root
    if (topo3d_rank == 0) {   
        double* C = (double*)malloc(sizeof(double) * (NX * NY * NZ));
        MPI_Gatherv(block_sums, BLOCKX * BLOCKY * BLOCKZ, MPI_DOUBLE, C, counts, disps, blocktype, 0, TOPOLOGY_3D);
	printf("Computational time: %f\n", MPI_Wtime()-start_time);
    }
    else {
        MPI_Gatherv(block_sums, BLOCKX * BLOCKY * BLOCKZ, MPI_DOUBLE, NULL, counts, disps, MPI_DOUBLE, 0, TOPOLOGY_3D);
    }

    free(recv_buffA);
    free(recv_buffB);
    free(block_sums);
    
    MPI_Finalize();
    return 0;
}
