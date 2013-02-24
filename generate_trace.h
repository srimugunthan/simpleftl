#include "stdio.h"
#include "stdlib.h"
#include "math.h"

typedef struct dist {
	double base;
	double mean;
	double var;
	int    type;
} synthio_distr;

typedef struct ioreq_ev {
	double time;
	int    type;
	struct ioreq_ev *next;
	struct ioreq_ev *prev;
	int    bcount;
	int    blkno;
	unsigned int  flags;
	unsigned int  busno;
	unsigned int  slotno;
	int    devno;
	int    opid;
	void  *buf;
	int    cause;
	int    tempint1;
	int    tempint2;
	void  *tempptr1;
	void  *tempptr2;
	void  *mems_sled;	 /* mems sled associated with a particular event */
	void  *mems_reqinfo; /* per-request info for mems subsystem */
	int    ssd_elem_num;	 /* SSD: element to which this request went */
	int    ssd_gang_num ; /* SSD: gang to which this request went */
	double start_time;    /* temporary; used for memulator timing */
	int    batchno;
	int    batch_complete;
	int    batch_size;
	struct ioreq_ev *batch_next;
	struct ioreq_ev *batch_prev;
} ioreq_event;

typedef struct gen {
	FILE          *tracefile;
	double         probseq;
	double         probloc;
	double         probread;
	double         probtmcrit;
	double         probtmlim;
	int            number;
	ioreq_event *  pendio;
//	sleep_event *  limits;
	int            numdisks;
	int            *devno;
	int            numblocks;
	int            sectsperdisk;
	int            blksperdisk;
	int            blocksize;
	synthio_distr  tmlimit;
	synthio_distr  genintr;
	synthio_distr  seqintr;
	synthio_distr  locintr;
	synthio_distr  locdist;
	synthio_distr  sizedist;
} synthio_generator;
