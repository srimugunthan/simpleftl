#ifndef MYNAND_H
#define MYNAND_H
#include "assert.h"
#include "string.h"

#define ASSERT(cond)   assert(cond)

#define MAX_ERASE_CYCLES 9999999
#define LB_SIZE_512  1
#define LB_SIZE_1024 2
#define LB_SIZE_2048 4

#define SECT_NUM_PER_PAGE  4
#define PAGE_NUM_PER_BLK  64
#define SECT_NUM_PER_BLK  (SECT_NUM_PER_PAGE * PAGE_NUM_PER_BLK)
#define SECT_SIZE_B 512

#define SECT_BITS       2
#define PAGE_BITS       6
#define PAGE_SECT_BITS  8
#define BLK_BITS       24

#define NAND_STATE_FREE    -1
#define NAND_STATE_INVALID -2

#define SECT_MASK_IN_SECT 0x0003
#define PAGE_MASK_IN_SECT 0x00FC
#define PAGE_SECT_MASK_IN_SECT 0x00FF
#define BLK_MASK_IN_SECT  0xFFFFFF00
#define PAGE_BITS_IN_PAGE 0x003F
#define BLK_MASK_IN_PAGE  0x3FFFFFC0

#define PAGE_SIZE_B (SECT_SIZE_B * SECT_NUM_PER_PAGE)
#define PAGE_SIZE_KB (PAGE_SIZE_B / 1024)
#define BLK_SIZE_B  (PAGE_SIZE_B * PAGE_NUM_PER_BLK)
#define BLK_SIZE_KB (BLK_SIZE_B / 1024)

#define BLK_NO_SECT(sect)  (((sect) & BLK_MASK_IN_SECT) >> (PAGE_BITS + SECT_BITS))
#define PAGE_NO_SECT(sect) (((sect) & PAGE_MASK_IN_SECT) >> SECT_BITS)
#define SECT_NO_SECT(sect) ((sect) & SECT_MASK_IN_SECT)
#define BLK_PAGE_NO_SECT(sect) ((sect) >> SECT_BITS)
#define PAGE_SECT_NO_SECT(sect) ((sect) & PAGE_SECT_MASK_IN_SECT)
#define BLK_NO_PAGE(page)  (((page) & BLK_MASK_IN_PAGE) >> PAGE_BITS)
#define PAGE_NO_PAGE(page) ((page) & PAGE_MASK_IN_PAGE)
#define SECTOR(blk, page) (((blk) << PAGE_SECT_BITS) | (page))

#define BLK_MASK_SECT 0x3FFFFF00
#define PGE_MASK_SECT 0x000000FC
#define OFF_MASK_SECT 0x00000003
#define IND_MASK_SECT (PGE_MASK_SECT | OFF_MASK_SECT)
#define BLK_BITS_SECT 22
#define PGE_BITS_SECT  6
#define OFF_BITS_SECT  2
#define IND_BITS_SECT (PGE_BITS_SECT + OFF_BITS_SECT)
#define BLK_F_SECT(sect) (((sect) & BLK_MASK_SECT) >> IND_BITS_SECT)
#define PGE_F_SECT(sect) (((sect) & PGE_MASK_SECT) >> OFF_BITS_SECT)
#define OFF_F_SECT(sect) (((sect) & OFF_MASK_SECT))
#define PNI_F_SECT(sect) (((sect) & (~OFF_MASK_SECT)) >> OFF_BITS_SECT)
#define IND_F_SECT(sect) (((sect) & IND_MASK_SECT))
#define IS_SAME_BLK(s1, s2) (((s1) & BLK_MASK_SECT) == ((s2) & BLK_MASK_SECT))
#define IS_SAME_PAGE(s1, s2) (((s1) & (~OFF_MASK_SECT)) == ((s2) & (~OFF_MASK_SECT)))

typedef unsigned int u32;
typedef signed int s32;
typedef unsigned char u8;
typedef u32 blk_t;

struct blk_state {
    int free;
    int erasecycles;
};

/* physical sector number can be 2048*64*2048*4 = = 2^30*/
struct sect_state {
    u32 free  :  1;
    u32 valid :  1;
    u32 lsn   : 30;
};

struct nand_blk_info {
    struct blk_state state;                   // Erase Conunter
    struct sect_state sect[SECT_NUM_PER_BLK]; // Logical Sector Number
    s32 free_page_count : 10; // free page counter
    s32 invalid_sector_count : 10; // invalide page counter
    s32 last_written_page : 12; // last written page number
    int page_status[PAGE_NUM_PER_BLK];
};



class nand_flash
{
    private:
        
        struct nand_blk_info *nand_flash;
        u32 total_nand_blks, min_free_blks;
        u8  pb_size;
        u32 total_free_blks;
    
    public:
    
        int nand_init (u32 blk_num, u8 min_free_blk_num);
        u32 nand_get_free_blk ();
        u8 nand_page_write(u32 psn, u32 *lsns);
        u8 nand_page_read(u32 psn, u32 *lsns, u8 isGC); 
        void dump_nand_flash();
        void nand_invalidate (u32 psn, u32 lsn);
        int nand_valid_sector(u32 blkno,u32 sect_offset);
        int nand_get_numinv_sectors(u32 blkno);
        int nand_get_total_freeblks();
        u32 nand_get_lsn(u32 blkno,u32 sectoffsetno);
        int  is_blk_free(int blk_no);
        void nand_erase (u32 blk_no);
        u32 nand_get_erase_cycles(int blk_no);
        void  dump_wear_status();
};

#endif