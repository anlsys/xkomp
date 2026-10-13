# include <stdio.h>     // puts
# include <stdlib.h>    // calloc
# include <assert.h>

// number of iterations
# define N_ITER                 16

// number of blocks per mesh
# define BLOCKS_PER_MESH        16

// number of elements per block, for each dimension
# define SX                     4
# define SY                     4
# define SZ                     4
# define ELEMENTS_PER_BLOCK     (SX*SY*SZ)
# define IDX(X, Y, Z)           (SY * (X * SX + Y) + Z)

// types to use
# define REAL                   double
# define INDEX                  int

// a block in the mesh
typedef struct  block_t
{
    REAL temperature[ELEMENTS_PER_BLOCK];
    REAL pressure[ELEMENTS_PER_BLOCK];
    REAL mass[ELEMENTS_PER_BLOCK];

}               block_t;

// a mesh
typedef struct  mesh_t
{
    block_t blocks[BLOCKS_PER_MESH];

}               mesh_t;

static mesh_t *
initialize(void)
{
    mesh_t * mesh = (mesh_t *) calloc(1, sizeof(mesh_t));
    assert(mesh);
    return mesh;
}

static void
deinitialize(mesh_t * mesh)
{
    free(mesh);
}

static void
hydro(block_t * block)
{
    # pragma omp target teams distribute parallel for               \
        collapse(2) device(0) nowait                                \
        access(read:  block->temperature[ELEMENTS_PER_BLOCK])       \
        access(write: block->mass[ELEMENTS_PER_BLOCK])
    for (INDEX x = 0 ; x < SX ; ++x)
    {
        for (INDEX y = 0 ; y < SY ; ++y)
        {
            for (INDEX z = 0 ; z < SZ ; ++z)
            {
                // TODO: whatever
                block->mass[IDX(x,y,z)] = 2 * block->temperature[IDX(x,y,z)] + 1;
            }
        }
    }
}

static void
burn(block_t * block)
{
    # pragma omp target teams distribute parallel for               \
        collapse(2) device(0) nowait                                \
        access(read, write: block->pressure[ELEMENTS_PER_BLOCK])
    for (INDEX x = 0 ; x < SX ; ++x)
    {
        for (INDEX y = 0 ; y < SY ; ++y)
        {
            for (INDEX z = 0 ; z < SZ ; ++z)
            {
                // TODO: whatever
                block->pressure[IDX(x,y,z)] *= 2;
            }
        }
    }
}

static void
simulate(mesh_t * mesh)
{
    // simulate
    for (INDEX i = 0 ; i < N_ITER ; ++i)
    {
        // here, we could add:
        // # pragma omp taskgraph
        for (INDEX block_id = 0 ; block_id < BLOCKS_PER_MESH ; ++block_id)
        {
            block_t * block = mesh->blocks + block_id;
            hydro(block);
            burn(block);
        }
    }

    // read back on the host
    # pragma omp task access(read: mesh->blocks[0].mass[0:4])
        puts("Write-back mass[0:4] from the first block, to the host");

    # pragma omp taskwait
}

int
main(void)
{
    // team of thread
    # pragma omp parallel
    {
        // a single thread spawns tasks
        # pragma omp single
        {
            // initialize the mesh domain
            mesh_t * mesh = initialize();

            // run the simulation
            simulate(mesh);

            // deinitialize
            deinitialize(mesh);
        }

    } // all threads wait for tasks completions, workstealing

    return 0;
}
