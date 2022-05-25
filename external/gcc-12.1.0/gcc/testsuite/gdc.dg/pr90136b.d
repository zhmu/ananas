// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93038
// { dg-do compile }
// { dg-options "-fdump-tree-optimized" }
// { dg-final { scan-tree-dump-times "sum_array \\(array\\)" 1 "optimized"} }

import gcc.attributes;

@attribute("noinline") int sum_array(int[] input);

int sum_array(int[] input)
{
    int sum = 0;
    foreach (elem; input)
        sum += elem;
    return sum;
}

int test(int[] array)
{
    return sum_array(array);
}
