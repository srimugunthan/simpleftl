/* 
 * generate_synthetic workload
 * input parmaeter s:
 *

   Number of I/O requests to generate =  5000000,
   Maximum time of trace generated  =  5000000.0,

   Think time from call to request =  0.0,
   Think time from request to return =  0.0,
Generators = [
disksim_synthgen { # generator 0
# vp - 85% of ((1 MB - 16K) pages)

   Storage capacity per device  =  7018904,
   devices = [ org0 ],
   Blocking factor =  8,
   Probability of sequential access =  1.0,
   Probability of local access =  0.0,
   Probability of read access =  0.0,
   Probability of time-critical request =  0.0,
   Probability of time-limited request =  0.0,
   Time-limited think times  = [ normal, 30.0, 100.0  ],
   General inter-arrival times  = [ uniform, 0.0, 1.0  ],
   Sequential inter-arrival times  = [ uniform, 0.0, 1.0  ],
   Local inter-arrival times  = [ exponential, 0.0, 0.0  ],
   Local distances  = [ normal, 0.0, 40000.0  ],
   Sizes  = [ exponential, 0.0, 8.0  ]
} # end of generator 0
] # end of generator list



 * 
 * code taken from disksim 4.0 disksim_synthio.c
 * /*
 * DiskSim Storage Subsystem Simulation Environment (Version 4.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001-2008.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */
#include "time.h"
#include "generate_trace.h"
#include "ftl.h"
#include "mynand.h"



#define SYNTHIO_UNIFORM		0
#define SYNTHIO_NORMAL		1
#define SYNTHIO_EXPONENTIAL	2
#define SYNTHIO_POISSON		3
#define SYNTHIO_TWOVALUE	4


#define DISKSIM_drand48 drand48


#define WRITE		0x00000000
#define READ		0x00000001
#define TIME_CRITICAL	0x00000002
#define TIME_LIMITED	0x00000004
#define TIMED_OUT	0x00000008
#define HALF_OUT	0x00000010
#define MAPPED		0x00000020
#define READ_AFTR_WRITE 0x00000040
#define SYNC	        0x00000080
#define ASYNC	        0x00000100
#define IO_FLAG_PAGEIO	0x00000200
#define SEQ		0x40000000
#define LOCAL		0x20000000
#define BATCH_COMPLETE  0x80000000

int synthio_gens;
int synthio_gencnt;
int synthio_iocnt;
int synthio_endiocnt;
double synthio_endtime;
int synthio_syscalls;
double synthio_syscall_time;
double synthio_sysret_time;
float gen_sim_time;

float sim_time = 0.0;

static double synthio_get_uniform (synthio_distr *fromdistr)
{
	return(((fromdistr->var - fromdistr->base) * DISKSIM_drand48()) + fromdistr->base);
}


static double synthio_get_normal (synthio_distr *fromdistr)
{
	double y1, y2;
	double y = 0;

	while (y <= 0.0) {
		y2 = - log((double) 1.0 - DISKSIM_drand48());
		y1 = - log((double) 1.0 - DISKSIM_drand48());
		y = y2 - ((y1 - (double) 1.0) * (y1 - (double) 1.0)) / 2;
	}
	if (DISKSIM_drand48() < 0.5) {
		y1 = -y1;
	}
	return((fromdistr->var * y1) + fromdistr->mean);
}


static double synthio_get_exponential (synthio_distr *fromdistr)
{
	double dtmp;

	dtmp = log((double) 1.0 - DISKSIM_drand48());
	return((fromdistr->base - (fromdistr->mean * dtmp)));
}


static double synthio_get_poisson (synthio_distr *fromdistr)
{
	double dtmp = 1.0;
	int count = 0;
	double stop;

	stop = exp(-fromdistr->mean);
	while (dtmp >= stop) {
		dtmp *= DISKSIM_drand48();
		count++;
	}
	count--;
	return((double) count + fromdistr->base);
}


static double synthio_get_twovalue (synthio_distr *fromdistr)
{
	if (DISKSIM_drand48() < fromdistr->var) {
		return(fromdistr->mean);
	} else {
		return(fromdistr->base);
	}
}


static double synthio_getrand (synthio_distr *fromdistr)
{
	switch (fromdistr->type) {
		case SYNTHIO_UNIFORM:
			return(synthio_get_uniform(fromdistr));
		case SYNTHIO_NORMAL:
			return(synthio_get_normal(fromdistr));
		case SYNTHIO_EXPONENTIAL:
			return(synthio_get_exponential(fromdistr));
		case SYNTHIO_POISSON:
			return(synthio_get_poisson(fromdistr));
		case SYNTHIO_TWOVALUE:
			return(synthio_get_twovalue(fromdistr));
		default:
			fprintf(stderr, "Unrecognized distribution type - %d\n", fromdistr->type);
			exit(1);
	}
}

FILE *fp;

static int synthio_generatenextio (synthio_generator *gen)
{
   double type;
   double reqclass;
   int blkno;
   ioreq_event *tmp;

#if 0
   if ((simtime >= synthio_endtime) || (synthio_iocnt >= synthio_endiocnt))
      return 0;
#endif
   tmp = gen->pendio;
#if 0
   if (gen->tracefile) {
      gen->pendio = 
	(ioreq_event *)iotrace_get_ioreq_event(gen->tracefile, 
					       disksim->traceformat, 
					       tmp);
      if (gen->pendio) {
	 tmp->cause = gen->number;
	 /* tmp->time = 0; */
      } else {
	 fprintf(stderr, "Returning NULL event in synthio_generatenextio\n");
	 return 0;
      }
      return 1;
   }
#endif
					 
   type = DISKSIM_drand48();

   if ((type < gen->probseq) && 
       ((tmp->blkno + 2*tmp->bcount) < gen->blksperdisk)) {

     tmp->time = -1.0;

     while (tmp->time < 0.0) {
       tmp->time = synthio_getrand(&gen->seqintr);
     }

     tmp->flags = SEQ | (tmp->flags & READ);
     tmp->cause = gen->number;
     tmp->blkno += tmp->bcount;
   } 
   else if ((type < (gen->probseq + gen->probloc)) && 
	    (type >= gen->probseq)) {

     tmp->time = -1.0;
     while (tmp->time < 0.0) {
       tmp->time = synthio_getrand(&gen->locintr);
     }
     tmp->flags = LOCAL;
     tmp->cause = gen->number;
     blkno = gen->blksperdisk;
     while (((blkno + tmp->bcount) >= gen->blksperdisk) 
	    || (blkno < 0) 
	    || (tmp->bcount <= 0)) 
       {
	 blkno = tmp->blkno + 
	   (int)synthio_getrand(&gen->locdist) / gen->blocksize;
	 tmp->bcount = ((int) synthio_getrand(&gen->sizedist) + 
			gen->blocksize - 1) / gen->blocksize;
       }
     tmp->blkno = blkno;
     if (DISKSIM_drand48() < gen->probread) {
       tmp->flags |= READ;
     }
   } 
   else {
      tmp->time = -1.0;
      while (tmp->time < 0.0) {
	tmp->time = synthio_getrand(&gen->genintr);
      }
      tmp->flags = 0;
      tmp->cause = gen->number;
      tmp->devno = gen->devno[(int)(DISKSIM_drand48() * 
				    (double)gen->numdisks)];

      tmp->blkno = tmp->bcount = gen->blksperdisk;
      while (((tmp->blkno + tmp->bcount) >= gen->blksperdisk) || 
	     (tmp->bcount <= 0)) {

	tmp->blkno = (int) (DISKSIM_drand48() * (double)gen->blksperdisk);
	tmp->bcount = ((int) synthio_getrand(&gen->sizedist) + 
		       gen->blocksize - 1) / gen->blocksize;
      }

      if (DISKSIM_drand48() < gen->probread) {
	tmp->flags = READ;
      }
   }
   reqclass = DISKSIM_drand48() - gen->probtmcrit;

   if (reqclass < 0.0) {
     tmp->flags |= TIME_CRITICAL;
   } 
   else if (reqclass < gen->probtmlim) {
     tmp->flags |= TIME_LIMITED;
   }
   sim_time += tmp->time;
#if 1
  // fprintf (fp, "New request %d, time %f, devno %d, blkno %d, bcount %d, flags %x\n", synthio_iocnt, sim_time, tmp->devno, tmp->blkno, tmp->bcount, tmp->flags);
  
#endif

   return 1;
}



void synthio_generate_io_activity (synthio_generator *gen)
{
	
   //   fprintf (stderr, "simtime %f, endtime %f, iocnt %d, endiocnt %d\n", simtime, synthio_endtime, synthio_iocnt, synthio_endiocnt);
   
	while( (synthio_iocnt <= synthio_endiocnt))
	{
		synthio_iocnt++;
		synthio_generatenextio(gen);
		//synthio_appendio(gen->pendio);
		
	}
		printf("end of trace generation\n");
	
}

void synthio_initialize_generator(synthio_generator * gen)
{
	ioreq_event *tmp;
	
	//event *evptr;
	double reqclass;

   
	fp = fopen("synthio.trace","w");
	srand48(time(NULL));
#if 0   
	evptr = getfromextraq();
	evptr->time = 0.0;
	evptr->type = SYNTHIO_EVENT;
	evptr->next = NULL;
	procp->eventlist = evptr;
	
	if (gen == NULL) {
		fprintf(stderr, "Process with no synthetic generator in synthio_initialize\n");
		exit(1);
	}
#endif
						
#if 0
	tmp = (ioreq_event *) getfromextraq();
#else
	tmp = (ioreq_event *) malloc(sizeof(ioreq_event));
#endif
	tmp->time = -1.0;
	while (tmp->time < 0.0) {
		tmp->time = synthio_getrand(&gen->genintr);
	}
	tmp->flags = 0;
	tmp->cause = gen->number;
	tmp->devno = gen->devno[(int) (DISKSIM_drand48() * (double) gen->numdisks)];
	tmp->blkno = tmp->bcount = gen->blksperdisk;
	while (((tmp->blkno + tmp->bcount) >= gen->blksperdisk) || (tmp->bcount == 0)) {
		tmp->blkno = (int) (DISKSIM_drand48() * (double) gen->blksperdisk);
		tmp->bcount = ((int) synthio_getrand(&gen->sizedist) + gen->blocksize - 1) / gen->blocksize;
	}
	if (DISKSIM_drand48() < gen->probread) {
		tmp->flags |= READ;
	}
	reqclass = DISKSIM_drand48() - gen->probtmcrit;
	if (reqclass < 0.0) {
		tmp->flags |= TIME_CRITICAL;
	} else if (reqclass < gen->probtmlim) {
		tmp->flags |= TIME_LIMITED;
	}
	gen->pendio = tmp;
        sim_time = tmp->time;
#if 0
	synthio_appendio(tmp);
#endif

#if 1
	fprintf (fp, "Initialized %d time %f, devno %d, blkno %d, bcount %d, flags %x\n", synthio_iocnt, sim_time, tmp->devno, tmp->blkno, tmp->bcount, tmp->flags);
#endif

}
class page_ftl flash_device;

int end_condition()
{
    if(flash_device.nand_get_erase_cycles(0) >= 500)
    {
        printf("reached end");
        return 1;
    }
    else
        return 0;
    
}
        

int main()
{
	synthio_generator gen;
        ioreq_event ioreq;
        u32 sectno;
	/*
	sleep_event *  limits;
	
	
	initialize the generator
	generate next io
	synthio_generate_io_activity(&gen);
	*/
        gen.numdisks = 1;
    
        gen.numblocks = 2048*64;
        gen.sectsperdisk = 4*64*2048;
        gen.blksperdisk = 2048*64;
        gen.blocksize = 2048;
	
   
        synthio_endiocnt = 10000;
   
        gen.probseq = 0.0; /* Probability of sequential access */
        gen.probloc = 0.0; /*Probability of local access  */
        gen.probread = 0.5; /*Probability of read access*/
        /* always set time crit and time limit to zero*/
        gen.probtmcrit = 0.0; /*Probability of time-critical request */
        gen.probtmlim = 0.0; /*Probability of time-limited request*/
   
        /* distributions type can be SYNTHIO_NORMAL or SYNTHIO_EXPONENTIAL or SYNTHIO_POISSON or SYNTHIO_TWOVALUE or SYNTHIO_UNIFORM */

        gen.tmlimit.type = SYNTHIO_NORMAL;
        gen.tmlimit.mean = 30.0;
        gen.tmlimit.var  = 100.0;

        /*    General inter-arrival times */
        gen.genintr.type = SYNTHIO_UNIFORM;
        gen.genintr.base = 0.0;
        gen.genintr.var = 1.0;

        /* Sequential inter-arrival times */
        gen.seqintr.type = SYNTHIO_UNIFORM;
        gen.seqintr.base = 0.0;
        gen.seqintr.var = 1.0;

        /*  Local inter-arrival times */
        gen.locintr.type = SYNTHIO_EXPONENTIAL;
        gen.locintr.base = 0.0;
        gen.locintr.mean = 0.0;

        /* Local distances*/
        gen.locdist.type = SYNTHIO_UNIFORM ;
        gen.locdist.mean = 0.0;
        gen.locdist.var = 40000.0;

        /* sizes*/
        gen.sizedist.type = SYNTHIO_EXPONENTIAL;
        gen.sizedist.base = 0.0;
        gen.sizedist.mean = 8.0; 
	
    
				 
	synthio_iocnt = 0;
	gen.number = 0; /* what the is this?*/
	gen.devno = (int*)malloc(gen.numdisks * sizeof(int));
        gen_sim_time = 0.0;
	synthio_initialize_generator(&gen);
        flash_device.ftl_init(2048,50);
        while(!end_condition())
        {
            synthio_iocnt++;

            synthio_generatenextio(&gen);
            ioreq.time = gen_sim_time + gen.pendio->time;
            ioreq.blkno = gen.pendio->blkno;
            ioreq.bcount = gen.pendio->bcount;
            ioreq.flags= gen.pendio->flags;
            if(gen.pendio->flags & READ)
            {
                ioreq.type = 1;
            }
            else
            {
                ioreq.type = 0;
                sectno = gen.pendio->blkno << SECT_BITS;
                flash_device.ftl_write(sectno,4);
            }


            gen_sim_time = gen_sim_time + gen.pendio->time;
 
  
 
        }
        flash_device.dump_wear_status();
	return 0;
}

