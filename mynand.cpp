#include <stdio.h>
#include <stdlib.h>
#include "assert.h"

#include "mynand.h"
extern FILE *dbgfile;




int nand_flash::nand_init (u32 blk_num, u8 min_free_blk_num)
{
    u32 blk_no;
    int i;
    
    nand_flash = (struct nand_blk_info *)malloc((sizeof (struct nand_blk_info)) * (blk_num+min_free_blk_num));

    if (nand_flash == NULL) 
    {
        printf("nand flash : malloc fail\n");
        return -1;
    }
    
    memset(nand_flash, 0xFF, sizeof (struct nand_blk_info) *( blk_num+min_free_blk_num));
    total_nand_blks = blk_num+min_free_blk_num;

    pb_size = 1;
    min_free_blks = min_free_blk_num;
    for (blk_no = 0; blk_no < total_nand_blks; blk_no++)
    {
        nand_flash[blk_no].state.free = 1;
        nand_flash[blk_no].state.erasecycles = 0;
        nand_flash[blk_no].free_page_count = SECT_NUM_PER_BLK;
        nand_flash[blk_no].invalid_sector_count = 0;
        nand_flash[blk_no].last_written_page = -1;

    
        for(i = 0; i<SECT_NUM_PER_BLK; i++)
        {
            nand_flash[blk_no].sect[i].free = 1;
            nand_flash[blk_no].sect[i].valid = 0;
            nand_flash[blk_no].sect[i].lsn = -1;
        }

        for(i = 0; i < PAGE_NUM_PER_BLK; i++)
        {
            nand_flash[blk_no].page_status[i] = 1;/* 1 for free and 0 for not free*/
        }
    }
    total_free_blks = total_nand_blks;
    return 0;
}

u32  nand_flash::nand_get_free_blk () 
{
    u32 blk_no = -1, i;
    int found_free_blk = 0;
    u32 min_erased = MAX_ERASE_CYCLES;
    


    
   /*selecting the blk with the minimum erase cycles*/
    for(i = 0; i < total_nand_blks; i++) 
    {
        if (nand_flash[i].state.free == 1)
        {
            found_free_blk = 1;

            if ( nand_flash[i].state.erasecycles < min_erased )
            {
                blk_no = i;
                min_erased = nand_flash[i].state.erasecycles;
               
            }
        }
    }
    if(!found_free_blk)
    {
        printf("no free block left=%d",total_free_blks);
        dump_nand_flash();
       // ASSERT(0);
    }
    else
    {
        if(nand_flash[blk_no].free_page_count != SECT_NUM_PER_BLK)
        {
            printf("blkno %u  free_page_count %u\n",blk_no,nand_flash[blk_no].free_page_count);
        }
        ASSERT(nand_flash[blk_no].free_page_count == SECT_NUM_PER_BLK);
        ASSERT(nand_flash[blk_no].invalid_sector_count == 0);
        ASSERT(nand_flash[blk_no].last_written_page == -1);
        nand_flash[blk_no].state.free = 0;
        total_free_blks--;

        return blk_no;
    }
    

    return -1;
}

/**************** NAND PAGE WRITE **********************/
u8  nand_flash:: nand_page_write(u32 psn, u32 *lsns)
{
    blk_t pbn = BLK_F_SECT(psn);	// physical block number with psn
    u32  pin = IND_F_SECT(psn);	// sector index, page index is the same as sector index 
    int i, valid_sect_num = 0;

    u32 my_pbn, my_pin;
    my_pbn = psn/SECT_NUM_PER_BLK;
    my_pin = pin%SECT_NUM_PER_BLK;
    if(my_pbn != pbn || my_pin != pin)
    {
     printf("my pbn %u my pin %u\n",my_pbn, my_pin);
     printf("break pbn %d psn %d !\n",pbn,psn);
     exit(-1);
    }
    if(pbn >= total_nand_blks){
        printf("break pbn %d psn %d !\n",pbn,psn);
    }

    ASSERT(pbn < total_nand_blks);
    ASSERT(OFF_F_SECT(psn) == 0);

  
    
    for (i = 0; i <SECT_NUM_PER_PAGE; i++) 
    {
        if (lsns[i] != -1) 
        {
            
#if 0
            if(nand_flash[pbn].state.free == 1) 
            {
                printf("blk num = %d",pbn);
            }
#endif
            if(nand_flash[pbn].sect[pin + i].free != 1)              
            {
                
                printf("pbn = %d, pin = %d , pin+i = %d \n", pbn, pin, pin+i); 
            }     
            ASSERT(nand_flash[pbn].sect[pin + i].free == 1);
      
            nand_flash[pbn].sect[pin + i].free = 0;			
            nand_flash[pbn].sect[pin + i].valid = 1;			
            nand_flash[pbn].sect[pin + i].lsn = lsns[i];	
            nand_flash[pbn].free_page_count--;  
            nand_flash[pbn].last_written_page = pin + i;	
            valid_sect_num++;
        }
        else
        {
            printf("lsns[%d] do not have any lsn\n", i);
        }
    }
  
    ASSERT(nand_flash[pbn].free_page_count >= 0);
    return valid_sect_num;
}

/**************** NAND PAGE READ **********************/
u8  nand_flash::nand_page_read(u32 psn, u32 *lsns, u8 isGC)
{
    blk_t pbn = BLK_F_SECT(psn);	// physical block number	
    u32  pin = IND_F_SECT(psn);	// page index (within the block), here page index is the same as sector index
    u32  i,j, valid_sect_num = 0;

    if(pbn >= total_nand_blks){
        printf("psn: %d, pbn: %d, nand_blk_num: %d\n", psn, pbn, total_nand_blks);
    }

    ASSERT(OFF_F_SECT(psn) == 0);
   
    ASSERT(nand_flash[pbn].state.free == 0);	

    if (isGC == 1) 
    {
        for (i = 0; i < SECT_NUM_PER_PAGE; i++)
        {

            if((nand_flash[pbn].sect[pin + i].free == 0) &&
                (nand_flash[pbn].sect[pin + i].valid == 1)) 
            {
                lsns[valid_sect_num] = nand_flash[pbn].sect[pin + i].lsn;
                valid_sect_num++;
            }
        }

        
    }
    else 
    {
         // every sector should be "valid", "not free"   
        for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
            if (lsns[i] != -1) {

                ASSERT(nand_flash[pbn].sect[pin + i].free == 0);
                ASSERT(nand_flash[pbn].sect[pin + i].valid == 1);
                ASSERT(nand_flash[pbn].sect[pin + i].lsn == lsns[i]);
                valid_sect_num++;
            }
            else
            {
                printf("lsns[%d]: %d shouldn't be -1\n", i, lsns[i]);
                exit(0);
            }
        }
    }
  
   
  
    return valid_sect_num;
}

void  nand_flash::dump_nand_flash()
{
    int i,j;
    FILE *nandumpfile;
    nandumpfile = fopen("./nandump.out","w");
    for(i = 0; i < total_nand_blks;i++)
    {
        for(j = 0; j < SECT_NUM_PER_BLK;j++)
    
            fprintf(nandumpfile,"block %u page %u psector %d logsector %d \n",i,j/SECT_NUM_PER_PAGE,j,nand_flash[i].sect[j].lsn
               );
    }
    fclose(nandumpfile);
}


/**************** NAND INVALIDATE **********************/
void  nand_flash::nand_invalidate (u32 psn, u32 lsn)
{
    u32 pbn = BLK_F_SECT(psn);
    u32 pin = IND_F_SECT(psn);
    //if(pbn > total_free_blks ) return;

    if((!(pbn <= total_nand_blks)) || (!(nand_flash[pbn].sect[pin].free == 0)) || (!(nand_flash[pbn].sect[pin].valid == 1)))
    {
        printf("nand _invalidate\n");
        printf("psn %u lsn %d\n",psn,lsn);
        printf("pbn %u pin %d\n",pbn,pin);
        printf("my pbn %u my pin %u\n",psn/(64*4), psn%(64*4));
        printf("in nand mapped lsn %d\n",nand_flash[pbn].sect[pin].lsn);
    }
#if 1
    ASSERT(pbn <= total_nand_blks);
    ASSERT(nand_flash[pbn].sect[pin].free == 0);
    ASSERT(nand_flash[pbn].sect[pin].valid == 1);
#endif
    if(nand_flash[pbn].sect[pin].lsn != lsn)
    {
        printf("nand lsn != mapped lsn\n");
        printf("psn %u lsn %d\n",psn,lsn);
        printf("pbn %u pin %d\n",pbn,pin);
        printf("my pbn %u my pin %d\n",psn/(64*4), psn%(64*4));
        printf("in nand mapped lsn %d\n",nand_flash[pbn].sect[pin].lsn);
        dump_nand_flash();
        exit(-1);
    }
    
  
    nand_flash[pbn].sect[pin].valid = 0;
    nand_flash[pbn].sect[pin].lsn = -1;
    nand_flash[pbn].invalid_sector_count++;
#ifdef DEBUG
    fprintf(dbgfile,"      invaldate %d %d \n",pbn,nand_flash[pbn].invalid_sector_count);
#endif
    

    ASSERT(nand_flash[pbn].invalid_sector_count <= SECT_NUM_PER_BLK);

}

int  nand_flash:: nand_valid_sector(u32 blkno,u32 sect_offset)
{
    if( nand_flash[blkno].sect[sect_offset].valid  == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int  nand_flash::nand_get_numinv_sectors(u32 blkno)
{
    return nand_flash[blkno].invalid_sector_count;
    
}

int  nand_flash:: nand_get_total_freeblks()
{
    return total_free_blks;
}

u32  nand_flash::nand_get_lsn(u32 blkno,u32 sectoffsetno)
{
  
    return nand_flash[blkno].sect[sectoffsetno].lsn;
}


int   nand_flash::is_blk_free(int blk_no)
{
    if(nand_flash[blk_no].state.free == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    
    }
}



void  nand_flash::nand_erase (u32 blk_no)
{
    ASSERT(blk_no < total_nand_blks);
    ASSERT(nand_flash[blk_no].free_page_count <= SECT_NUM_PER_BLK);
    ASSERT(nand_flash[blk_no].state.free == 0);

    nand_flash[blk_no].state.free = 1;
    nand_flash[blk_no].state.erasecycles++;
    nand_flash[blk_no].free_page_count = SECT_NUM_PER_BLK;
    nand_flash[blk_no].invalid_sector_count = 0;
    nand_flash[blk_no].last_written_page = -1;

    int i;
    for(i = 0; i<SECT_NUM_PER_BLK; i++)
    {
        nand_flash[blk_no].sect[i].free = 1;
        nand_flash[blk_no].sect[i].valid = 0;
        nand_flash[blk_no].sect[i].lsn = -1;
    }
    for(i = 0; i < PAGE_NUM_PER_BLK; i++)
    {
        nand_flash[blk_no].page_status[i] = -1;
    }
    total_free_blks++;
 
}
u32  nand_flash::nand_get_erase_cycles(int blk_no)
{
    return  nand_flash[blk_no].state.erasecycles;
}

void  nand_flash:: dump_wear_status()
{
    FILE *fp;
    int i;
    fp = fopen("./wearstatus.out", "w");
    for(i = 0; i < total_nand_blks;i++)
    {
        fprintf(fp,"%d %u\n", i, nand_flash[i].state.erasecycles);
    }
    fclose(fp);
}
