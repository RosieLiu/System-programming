This test program is a sample correctness test case for the thread
safe malloc implementation version. Note that this test does not 
*guarantee* that your code is completely correct. But if your code
does pass this test, it is likely at least in pretty good shape.

When executed, the program will either print (on test success):
No overlapping allocated regions found!
Test passed

Or on test failure, something like:
Found 2 overlapping allocated regions.
Region 1 bounds: start=ca0010, end=ca0090, size=128B, idx=0
Region 2 bounds: start=ca0010, end=ca01b0, size=416B, idx=30
Test failed

In the fail output, you can see here that malloc returned the same
starting address for two different malloc calls.

This program works by creating N threads (using pthreads). Each
thread reaches a barrier at the start of its execution function
to give all threads time to get started. Then each thread loops
and mallocs memory regions of random sizes, and then exits.

The program checks correctness by then looking at the address 
ranges of all malloc'ed regions to make sure that they are all
completely distinct (test pass).  If any regions overlap, then
the test fail message is printed.

There are two #define values in the c code that you may change
to easily change the number of threads or number of items each
thread will malloc:
#define NUM_THREADS  4
#define NUM_ITEMS    10

To compile this program, you may work with the provided Makefile.
There is one variable that you will need to edit:

1) WDIR should point to the directory with your my_malloc.* code
and compiled library (libmymalloc.so).


