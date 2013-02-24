#define INVERSE_MAP 1

#include "ftl.h"
#include "mynand.h"
#include "stdio.h"
#include "stdlib.h"

#define DEBUG1 1

#ifdef DEBUG1
FILE *dbgfile;
#endif 
FILE *gcdbgfile;
FILE *freeblkinfo;

typedef int sect_t;

#define GC_THRESH_BLKS 512
#define MIN_ERASED_SELECT 1

#define SUCCESS 1


struct freeblkinfo cur_written;

u32 page_ftl::get_free_blk()
{
    static int count = 0;
    
    int blkno;
    u32 lpn,psn;
    int i;
   
    /* selecting a next blk to write*/
    if(blk_select_policy == MIN_ERASED_SELECT)
    {
     
        blkno =  nand_get_free_blk();
#if 0
        fprintf(freeblkinfo,"ftl getfreeblk for %d time \n",++count);
        fprintf(freeblkinfo,"returning blk %d\n",blkno);
#endif

        return blkno;
    }
}


u32 page_ftl:: get_next_free_page()
{
    u32 freeblk;
    u32 page_no;

    if(cur_written.page_offset < PAGE_NUM_PER_BLK-1)
    {
         
        cur_written.page_offset += 1;
        cur_written.sector_offset = 0;
        if(nand_valid_sector(cur_written.free_blk ,cur_written.page_offset*SECT_NUM_PER_PAGE) || is_blk_free(cur_written.free_blk))
        {
            printf("something wrong \n");
            printf("curblk %u pageoffset %u\n",cur_written.free_blk ,cur_written.page_offset);
            exit(-1);
        }
         
    }
    else
    {

        freeblk = get_free_blk();
        if(freeblk == -1)
        {
            printf("unexpected: getfreepage no free blk");
            if( ingc_phase == 1)
            {
                printf("failed in GC PHASE\n");
            }
            exit(-1);
        }
        else
        {
            cur_written.free_blk = freeblk;
            cur_written.page_offset = 0;
            cur_written.sector_offset = 0;
            update_freespace(-1);
            
        }
        
    }
 
    page_no = (cur_written.free_blk*PAGE_NUM_PER_BLK)+ cur_written.page_offset;
    return page_no;
    
}


int page_ftl::ftl_init(u32 num_blks, u32 extra_num)
{
    int i;

    ftl_total_pages = num_blks * PAGE_NUM_PER_BLK;
    
    ftl_total_nand_blks = num_blks+extra_num;
#ifdef INVERSE_MAP
    /*reset to a different value for inverse map*/
    ftl_total_pages = ftl_total_nand_blks * PAGE_NUM_PER_BLK;
#endif
    extra_blks = extra_num;
#ifdef DEBUG1
    dbgfile = fopen("./debug.out","w");
    freeblkinfo = fopen("./fblkinfo.out","w");
#endif

    

    nand_init(num_blks,extra_blks);
    
    pagemap = (struct pm_entry *) malloc((sizeof(struct pm_entry)) * ftl_total_pages);
    if ((pagemap == NULL) ) {
        printf("pagemap tablle malloc failed\n");
        return -1;
    }
    memset(pagemap, 0xFF, sizeof (struct pm_entry) * ftl_total_pages);
   
    for(i = 0; i<ftl_total_pages; i++){
     
        pagemap[i].map_status = 0;
        pagemap[i].map_age = 0;
    }
    
    blk_select_policy = MIN_ERASED_SELECT;
    
    cur_written.free_blk = get_free_blk();
    cur_written.page_offset = -1;
    cur_written.sector_offset = 0;
    

    ftl_total_free_blks = nand_get_total_freeblks();
    return 0;
}




#if 1
int thresh_table[][2] = {
    0,4,
    10,8,
    50, 10,
    100,20,
    125,25,
    150,30,
    175,40,
    200,60,
    225,80,
    255,100,
    512,128,
    -1,-1
            
};
#else


int thresh_table[][2] = {
    0,4,
    2,10,
    4, 30,
    6, 50,
    8, 80,
    10,90,
    -1,-1
            
};

#endif

   

   
void page_ftl::update_freespace(int blks)
{
   u32 threstab_value; 
   int i;
   
   ftl_total_free_blks = nand_get_total_freeblks();
      
   threstab_value =  (ftl_total_free_blks - extra_blks-1);
   if(threstab_value > GC_THRESH_BLKS)
    {
        return;
    }
    
   for(i = 0;threstab_value > thresh_table[i][0];i++)
    {
        
        if(thresh_table[i][0] == -1)
        {
            printf("unexpected: threshtab reached end");
            printf("threshtabval %d\n",threstab_value);
            exit(-1);
        }
    }
    cur_invsectors_gcthresh = thresh_table[i][1];

}




int page_ftl::merge_with(int src_blkno)
{
    int i,j;
    u32 ppn_to_write,psn_to_write;
    u32 lsns[SECT_NUM_PER_PAGE];
    u32 lsn;
    u32 lpn;
    int num_invsectors;
    int old_ppn;
    num_invsectors = nand_get_numinv_sectors(src_blkno);
#ifdef GC_DEBUG
    fprintf(dbgfile,"GC merge with %d numvalidsectors %d \n",src_blkno, SECT_NUM_PER_BLK-num_invsectors);
#endif
    
    
    
    if(num_invsectors == SECT_NUM_PER_BLK)
    {
        nand_erase(src_blkno);
    }
    else
    {
      
        for(i = 0; i < SECT_NUM_PER_BLK ;i+=4)
        {
            /*
             * since we write in units of page,
             * we just check if the first sector
             * of the page is valid
             */
            if(nand_valid_sector(src_blkno,i))
            {
                /* nand read the logical sector number*/
                memset (lsns, 0xFF, sizeof (lsns));
                lsn = nand_get_lsn(src_blkno,i);
                old_ppn = (((src_blkno * SECT_NUM_PER_BLK) + i)/SECT_NUM_PER_PAGE);
               
                for (j = 0; j < SECT_NUM_PER_PAGE; j++) 
                {
                    lsns[j] = lsn + j;
                }
                
                

                ppn_to_write = get_next_free_page();
                
                if(ppn_to_write == -1)
                {
                    printf("merge: unexpected: no page to write");
                    exit(-1);
                }
                /* repagemap */
                lpn = lsn / SECT_NUM_PER_PAGE;
                
                /* mapchange 1*/
#ifdef INVERSE_MAP
                if(pagemap[old_ppn].lpn != lpn || pagemap[old_ppn].free != 0)
                {
                    printf("old ppn doesnt have it right \n");
                    exit(-1);
                }
              
                pagemap[ppn_to_write].lpn = lpn;
                pagemap[ppn_to_write].free = 0;
                pagemap[old_ppn].lpn = -1;
                pagemap[old_ppn].free = 1;
#else
                pagemap[lpn].ppn = ppn_to_write;
                pagemap[lpn].free = 0;
            
#endif
            
                /* do actual write*/
                psn_to_write = ppn_to_write * (SECT_NUM_PER_PAGE);
              
                nand_page_write(psn_to_write,lsns);
                
            }
        }
        nand_erase(src_blkno);    
        
    }
    return SUCCESS;
        
}
 

#if 1

void page_ftl::cleanup_blks(int cur_invsectors_gcthresh)
{
    
    int merged_blks = 0;
    int i;
    int num_invsectors;
    int maxinvsectorsinblk;
#ifdef DEBUG1
    fprintf(dbgfile, "GC  free blocks %d\n", ftl_total_free_blks);
#endif
#ifdef DEBUG1
    fprintf(dbgfile, "   cur_invsectors_gcthresh %d\n", cur_invsectors_gcthresh);
#endif
    for(i = 0; i < ftl_total_nand_blks;i++)
    {
        num_invsectors = nand_get_numinv_sectors(i);
        if( num_invsectors > cur_invsectors_gcthresh)
        {
            if(merge_with(i) == SUCCESS)
            {
                merged_blks++;
                 
            }
            else
            {
                printf("unexpected: merge fail");
                exit(-1);
            }
            
        }
        
    }
    update_freespace(merged_blks);
    
    if(merged_blks == 0)
    {
        
#ifdef DEBUG1
        fprintf(dbgfile, "GC fail no blks gained \n");
#endif
        maxinvsectorsinblk = 0;
        for(i = 0; i < ftl_total_nand_blks;i++)
        {
            num_invsectors = nand_get_numinv_sectors(i);
            if(maxinvsectorsinblk < num_invsectors)
                maxinvsectorsinblk = num_invsectors;
        }
#ifdef DEBUG1            
        fprintf(dbgfile," max invsects %d \n",maxinvsectorsinblk);
#endif
       // exit(-1);
    }
#ifdef DEBUG1
    fprintf(dbgfile, "GC  return  total free blocks %d\n", ftl_total_free_blks);
#endif

}
#else
                              
void cleanup_blks(int cur_invsectors_gcthresh)
{
    
    int merged_blks = 0;
    int i;
    int num_invsectors;
    int maxinvsectors;
    int count = 0;
    int targetblk;
    int gained_blks = 0;
    int initial_freeblks;
    int another_count =0;
    int maxinvsectorsinblk;
#ifdef DEBUG1
    fprintf(dbgfile, "GC cleanup with total free blocks %d\n", ftl_total_free_blks);
#endif
    initial_freeblks = nand_get_total_freeblks();
    while(gained_blks <= 1 && another_count < ftl_total_nand_blks/4)
    {
        /* try atleast 4 blks*/
        while(count < 4)
        {
            maxinvsectors = 0; targetblk = -1;
            for(i = 0; i < ftl_total_nand_blks;i++)
            {
                
                /*use greedy policy , select a blk with the most invalid sectors*/
                num_invsectors = nand_get_numinv_sectors(i);    
                if(num_invsectors > maxinvsectors)
                {
                    targetblk = i;
                    maxinvsectors = num_invsectors;
                }
                
            }
            if(maxinvsectors > 10)
            {
                if(merge_with(targetblk) == SUCCESS)
                {
                    merged_blks++;
                            
                }
                else
                {
                    printf("unexpected: merge fail");
                    exit(-1);
                }
            }
            count++;
        }
        gained_blks =  nand_get_total_freeblks() - initial_freeblks;
        another_count++;
    }
    update_freespace(merged_blks);
    if(gained_blks == 0)
    {
#ifdef DEBUG1
        

        fprintf(dbgfile, "GC hopeless no blks gained \n");
#endif
    }
    if(merged_blks == 0)
    {
#ifdef DEBUG1
        fprintf(dbgfile, "worse, no blks merged \n");
#endif
        maxinvsectorsinblk = 0;
        
        for(i = 0; i < ftl_total_nand_blks;i++)
        {
            num_invsectors = nand_get_numinv_sectors(i);
            if(maxinvsectorsinblk < num_invsectors)
                maxinvsectorsinblk = num_invsectors;

        }
#ifdef DEBUG1
        fprintf(dbgfile,"blk maxinvsects %d \n", maxinvsectorsinblk);
#endif

    }
#ifdef DEBUG1
    fprintf(dbgfile, "GC cleanup return with total free blocks %d\n", ftl_total_free_blks);
#endif

}
#endif
                             
void page_ftl::chk_gcollection()
{
    
    /* just check if you have to do any gcollection*/
    if((ftl_total_free_blks-extra_blks) < GC_THRESH_BLKS )
    {
        ingc_phase = 1;
        /* cleanup blks with more than cur_num_invalidpages*/
        cleanup_blks(cur_invsectors_gcthresh);
        ingc_phase = 0;
    }
}
#ifdef INVERSE_MAP
/* mapchange 2*/
int page_ftl::search_pagemap(int lpn)
{
    int i;
    for(i = 0; i < ftl_total_pages;i++)
    {
        //if(!(pagemap[i].free))
        {
             
            if(pagemap[i].lpn ==  lpn )
            {
                if(pagemap[i].free != 0)
                {
                    printf("pagemap wrong entry\n");
                    exit(-1);
                }
                return i;
            }
        }
    }
    return -1;
}
#endif

u32 page_ftl::ftl_invalidate(int lsn, int num_sectors)  
{
    int lpn; 
    int psn;
    int i,j;
    u32 lsns[SECT_NUM_PER_PAGE];
    int num_pages;
    int index;
    printf( "ftl_invalidate lsn %d\n", lsn);
    num_pages = num_sectors/SECT_NUM_PER_PAGE;
    
    for(j = 0; j < num_pages ; j++)
    {
        lpn = (lsn/ (SECT_NUM_PER_PAGE));
        index = search_pagemap(lpn);
	if(index == -1)
	{
		printf("problem, page %d not in stgserver",lpn);
		exit(-1);
	}
        psn = (index)* (SECT_NUM_PER_PAGE);
        for(i = 0; i<SECT_NUM_PER_PAGE; i++){
            nand_invalidate(psn + i, lsn + i);
        }
        lsn += SECT_NUM_PER_PAGE;
        pagemap[index].free =  1;
        pagemap[index].lpn = -1;
    } 
    
}

/* The algorithm :
 *  
 *  do a preliminary gcollection check
 *  do an invalidate 
 *  get a free page 
 *  remap the sector to write 
 *  do a raw flash write
 */

/* presently num_sectors should be a multiple of SECT_NUM_PAGE*/


u32 page_ftl::ftl_write(int lsn, int num_sectors)  
{
    int i;
    int lpn; 
    int num_pages; 
    int ppn;
    u32 lsns[SECT_NUM_PER_PAGE];
    int psn;
    int offset_in_page;
    int j;
    u32 psn_to_write,ppn_to_write;
    static int gcollect_trigger_count = 0;
    int index;
    
    int count;
#ifdef DEBUG
  
    
    fprintf(dbgfile, "ftl_write lsn %d\n", lsn);
#endif
   
#if 0
if(lsn >= SECT_NUM_PER_BLK*ftl_total_nand_blks)
{
printf("lsn %d ftltotalblks %d\n",lsn,ftl_total_nand_blks);
}
    ASSERT(lsn < SECT_NUM_PER_BLK*ftl_total_nand_blks);
#endif
    lpn = (lsn/ (SECT_NUM_PER_PAGE));
#ifdef DEBUG
    fprintf(dbgfile, "       lpn %d\n", lpn);
#endif
    offset_in_page = lsn%SECT_NUM_PER_PAGE;
    if(offset_in_page != 0)
    {
        printf("ftl_write:expecting in multiples of num_sectors in a page");
        exit(-1);
    }
    rewrite_lsn = lsn;
    num_pages = num_sectors/SECT_NUM_PER_PAGE;   	
    ASSERT(lpn < ftl_total_pages);
    ASSERT(lpn + num_pages <= ftl_total_pages);

    gcollect_trigger_count++;
    if(gcollect_trigger_count >=  64*1)
    {
        /* do a gcollection check*/
        chk_gcollection();
        gcollect_trigger_count = 0;
    }
   
    for(j = 0; j < num_pages ; j++)
    {
        lpn = lpn + j;
#ifdef INVERSE_MAP
        index = search_pagemap(lpn);     
        if(index == -1) 
#else
            if(pagemap[lpn].free) 
#endif
        {
            /*  previous mapping happens to be free*/
#ifdef DEBUG
            fprintf(dbgfile,"       prevmap free, safe if it is first write!!\n");
#endif

          
            
            /* probably first time, use getfreepage to get another page to write*/
            ppn_to_write = get_next_free_page();
#ifdef DEBUG
            fprintf(dbgfile, "     towrite ppn %d\n",ppn_to_write);
#endif
            if(ppn_to_write == -1)
            {
                printf("unexpected: no page to write");
                exit(-1);
            }
            /* mapchange 3*/
#ifdef INVERSE_MAP

            pagemap[ppn_to_write].free = 0;
            pagemap[ppn_to_write].lpn = lpn;
            
       
#else
            pagemap[lpn].free = 0; /* no more*/
            pagemap[lpn].ppn = ppn_to_write;
            
#endif
            
        }
        else
        {
#ifdef INVERSE_MAP
            psn = (index)* (SECT_NUM_PER_PAGE);
#else
            psn = ((pagemap[lpn].ppn)* (SECT_NUM_PER_PAGE));
#endif
            
            /* we invalidate here to let the later gcollection to cleanup*/
#ifdef DEBUG
       //     fprintf(dbgfile,"       prevmap ppn %d\n",(pagemap[lpn].ppn));
#endif
            for(i = 0; i<SECT_NUM_PER_PAGE; i++){
                nand_invalidate(psn + i, lsn + i);
            } 
            ppn_to_write = get_next_free_page();
#ifdef DEBUG  
            fprintf(dbgfile,"      towrite ppn %d\n",ppn_to_write);
#endif
            if(ppn_to_write == -1)
            {
                printf("unexpected: no page to write");
                exit(-1);
            }
#ifdef INVERSE_MAP
            /* mapchange 4*/
            pagemap[index].lpn = -1;
            pagemap[index].free = 1;
            pagemap[ppn_to_write].lpn = lpn;
            pagemap[ppn_to_write].free = 0;
#else
            pagemap[lpn].ppn = ppn_to_write;
#endif
        }
        
        memset (lsns, 0xFF, sizeof (lsns));
    
        for (i = 0; i < SECT_NUM_PER_PAGE; i++) 
        {
            lsns[i] = lsn + i;
        }

        psn_to_write = ppn_to_write * (SECT_NUM_PER_PAGE);
#ifdef DEBUG
        fprintf(dbgfile,"      towrite lsn %d psn %d\n",lsns[0], psn_to_write);
#endif
        nand_page_write(psn_to_write, lsns);
        
    }
    if(ftl_total_free_blks != nand_get_total_freeblks())
    {
        update_freespace(ftl_total_free_blks - nand_get_total_freeblks());
    } 
#ifdef DEBUG
    fprintf(dbgfile, "ftl_write lsn %d finished\n", lsn);
#endif
}
