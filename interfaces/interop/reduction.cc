float * a;
float total;

for (int j = 0 ; j < nparts ; ++j)
{
    int start = (j+0) * part_size;
    int   end = (j+1) * part_size;
    # pragma omp target task access(read: segment(a, start, end)) reduction(+: total)
    {
        float local = 0.0;
        for (int i = start; i < end; i++)
            local += a[i];
        total += local;
    }
}
