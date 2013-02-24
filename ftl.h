#ifndef FTL_H
#define FTL_H
#include "mynand.h"

struct freeblkinfo
{
    u32 free_blk;
    int page_offset;
    u32 sector_offset;
};

/* mapchange 0*/
struct pm_entry {

#ifdef INVERSE_MAP
    int free;
    int lpn;
#else
    int free;
    int ppn ;
#endif
    int  map_status;
    int  map_age;

};

class page_ftl: public nand_flash
{
    private:
       int rewrite_lsn;

       int cur_invsectors_gcthresh;
       struct pm_entry *pagemap;

       u32 ftl_total_pages;
       int blk_select_policy;
       int extra_blks;
       int ftl_total_nand_blks;
       int ftl_total_free_blks;
       int ingc_phase;
        
    public:
        struct freeblkinfo cur_written;
        void update_freespace(int blks);
        u32 ftl_write(int lsn, int num_sectors);
        u32 ftl_invalidate(int lsn, int num_sectors);  
        void chk_gcollection();
        int search_pagemap(int lpn);
        
        void cleanup_blks(int cur_invsectors_gcthresh);
        int merge_with(int src_blkno);
        int ftl_init(u32 num_blks, u32 extra_num);
        u32 get_next_free_page();
        u32 get_free_blk();
};
#endif
