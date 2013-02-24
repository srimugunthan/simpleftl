simpleftl
=========
simple FTL over simulated nand.

NAND flash is simulated as an array with statistics stored for every block.

A simple page based FTL operates over the NAND flash.

Can be used for experimenting with  algorithms for FTL.

for input trace generation, in main.cpp, we re-use some code from disksim source(www.pdl.cmu.edu/DiskSim/)

COMPILE:
=======
g++ ftl.cpp mynand.cpp main.cpp






