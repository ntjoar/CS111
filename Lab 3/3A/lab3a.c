#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>

#include "fs.h"

#define SYM_LINK 0xA000
#define REG_FILE 0x8000
#define DIR_FILE 0x4000

const int SB = 1024;

struct ext2_super_block sb;
__u32 blSize;

int groupc = 1;
struct ext2_group_desc* gdArr;
int imgFD;

void groupPrint(int num) {
    int blockNum;
    if ( sb.s_blocks_per_group >  sb.s_blocks_count)
        blockNum=sb.s_blocks_count;
    else
        blockNum=sb.s_blocks_per_group;
    
    printf("%s,%u,%u,%u,%u,%u,%u,%u,%u\n", "GROUP",
           num, blockNum, sb.s_inodes_per_group, gdArr[num].bg_free_blocks_count, gdArr[num].bg_free_inodes_count,
           gdArr[num].bg_block_bitmap, gdArr[num].bg_inode_bitmap, gdArr[num].bg_inode_table);
}

int getGroupOffset(int n) {
    return SB + (int) blSize + n * sizeof(struct ext2_group_desc);
}

int getBlockOffset(int blID) {
    return blID * (int) blSize;
}

int getIndirOffset(int n) {
    return blSize*n;
}

int getDirOffset(int inIndex, int logical_offset) {
    return inIndex*blSize+logical_offset;
}

char getfType(__u16 i_mode) {
    char type;
    __u16 msk = 0xF000;
    __u16 format = i_mode & msk;
    
    switch(format) {
        case SYM_LINK:
            type = 'l';
            break;
        case REG_FILE:
            type = 'f';
            break;
        case DIR_FILE:
            type = 'd';
            break;
        default:
            type = '?';
            break;
    }
    return type;
}

void procBm(int n) {
    __u32 id = gdArr[n].bg_block_bitmap;
    char* bm = malloc(blSize * sizeof(char));
    if (pread(imgFD, bm, blSize, getBlockOffset(id)) < 0) {
        fprintf(stderr, "Error: cannot from the bitmap\n");
        exit(1);
    }

    unsigned i;
    int j;
    int k = 1;
    for (i = 0; i < blSize; i++) {
        char c = bm[i];
        for (j = 0; j < 8; ++j) {
            int b = c & (k << j);
            int blockN = n * sb.s_blocks_per_group + i * 8 + j + 1;
            if (b == 0) {
                printf("%s,%u\n", "BFREE", blockN);
            }
        }
    }
}

void printIndRef(__u32 blockNum, int inodeNum, int offset, int lvl) {
    unsigned i;
    __u32 i_blockNum=blSize/4;
    __u32 *ele=malloc(blSize*sizeof(__u32));

    if(ele == NULL) {
        fprintf(stderr, "Error: unable to allocate memory for ele\n");
        free(ele);
        exit(1);
    }
    
    if (pread(imgFD, ele, blSize, getIndirOffset(blockNum)) < 0) { 
        fprintf(stderr, "Error: cannot read from the indirect block\n");
        exit(1);
    }
    
    for (i = 0; i < i_blockNum; i++) { 
        if(ele[i] != 0) {
            printf("%s,%d,%d,%d,%u,%u\n", "INDIRECT",
                   inodeNum, lvl, offset, blockNum, ele[i]);
            if (lvl == 2 || lvl == 3)
                printIndRef(ele[i], inodeNum, offset, lvl-1);
        }
        
        if (lvl == 1) {
            offset++;
        } else if (lvl == 2){
            offset += 256;
        } else if (lvl == 3) {
            offset += (256 * 256);
        }
    }
    
    free (ele);
}

void printDirent(struct ext2_inode* in, int inodeNum, int logOff) {
    struct ext2_dir_entry *dirent=malloc(sizeof(struct ext2_dir_entry));
    unsigned i;
    if(dirent == NULL) {
        fprintf(stderr, "Error: unable to allocate memory for dirent\n");
        free(dirent);
        exit(1);
    }

    for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if(in->i_block[i] != 0) {
            while((unsigned) logOff < blSize) {
                if (pread(imgFD, dirent, sizeof(struct ext2_dir_entry), 
                    getDirOffset(in->i_block[i],logOff)) < 0) {
                        fprintf(stderr, "Error: cannot from the directory entries\n");
                        exit(1);
                    }
                if(dirent->inode!=0) {
                    printf("%s,%u,%d,%u,%u,%u,'%s'\n", "DIRENT",
                           inodeNum, logOff,
                           dirent->inode, dirent->rec_len,
                           dirent->name_len, dirent->name);
                }
                logOff+=dirent->rec_len;
            }
        }
    }   

    free(dirent);
}

void procInbm(int n) {
    __u32 bmNum = gdArr[n].bg_inode_bitmap;
    char* bm = malloc(blSize * sizeof(char));

    if(bm == NULL) {
        fprintf(stderr, "Error: unable to allocate memory for bm\n");
        free(bm);
        exit(1);
    }

    if (pread(imgFD, bm, blSize, getBlockOffset(bmNum)) < 0) {
        fprintf(stderr, "Error: cannot from the bitmap\n");
        exit(1);
    }
    
    unsigned i;
    int j;
    int k = 0x01;
    for (i = 0; i < blSize; i++) {
        char c = bm[i];
        for (j = 0; j < 8; ++j) {
            int b = c & (k << j);
            int inodeN = n * sb.s_inodes_per_group + i * 8 + j + 1;
            if (b == 0) {
                printf("%s,%u\n", "IFREE", inodeN);
            }
        }
    }
    free(bm);
}

void decodeTime(char* buf, __u32 i_time) {
    struct tm* time_gm;
    time_t tmp = i_time;
    
    time_gm = gmtime(&tmp);
    if (time_gm == NULL) {
        fprintf(stderr, "Error decoding inode time to GMT\n");
        exit(2); //?
    }

    sprintf(buf, "%02d/%02d/%02d %02d:%02d:%02d",  time_gm->tm_mon + 1, time_gm->tm_mday,
            (time_gm->tm_year) % 100, time_gm->tm_hour, time_gm->tm_min, time_gm->tm_sec);
}

void procIn(int n) {
    __u32 inodeTableN = gdArr[n].bg_inode_table;
    struct ext2_inode* inodeArr;
    size_t inodeArrSz = sb.s_inodes_per_group * sizeof(struct ext2_inode);
    inodeArr = (struct ext2_inode*) malloc(inodeArrSz);
    
    if(inodeArr == NULL) {
        fprintf(stderr, "Error: unable to allocate memory for inodeArr\n");
        free(inodeArr);
        exit(1);
    }

    if (pread(imgFD, inodeArr, inodeArrSz, getBlockOffset(inodeTableN)) < 0) {
        fprintf(stderr, "Error reading from the inode table\n");
        exit(1);
    }
    
    unsigned i;
    int j;
    for (i = 0; i < sb.s_inodes_per_group; i++) {
        struct ext2_inode cur = inodeArr[i];
        int inodeN =  n * sb.s_inodes_per_group + i + 1;
        if (cur.i_mode != 0 && cur.i_links_count != 0) {
            char fType = getfType(cur.i_mode);
            __u16 msk = 0xFFF;
            __u16 mode = cur.i_mode & msk;
            char ctime_str[30];
            char mtime_str[30];
            char atime_str[30];
            
            decodeTime(ctime_str, cur.i_ctime);
            decodeTime(mtime_str, cur.i_mtime);
            decodeTime(atime_str, cur.i_atime);
            
            printf("INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u", inodeN, 
                   fType, mode,  cur.i_uid, cur.i_gid, cur.i_links_count, 
                   ctime_str, mtime_str, atime_str, cur.i_size, cur.i_blocks);
            
            for (j = 0; j < 15; j++) {
                __u32 block_num = cur.i_block[j];
                printf(",%u",block_num);
            }
            printf("\n");
            
            if (fType == 'd') {
                printDirent(&cur, inodeN, 0);
            }
            
            if (cur.i_block[EXT2_IND_BLOCK] != 0){ 
                printIndRef(cur.i_block[EXT2_IND_BLOCK] ,inodeN, 12, 1);    
            }
            if (cur.i_block[EXT2_DIND_BLOCK] != 0){ 
                printIndRef(cur.i_block[EXT2_DIND_BLOCK], inodeN, 268, 2);
            }
            if (cur.i_block[EXT2_TIND_BLOCK] != 0) { 
                printIndRef(cur.i_block[EXT2_TIND_BLOCK], inodeN, 65804, 3);  
            }
            
        }
    }
    free(inodeArr);
}

int main(int argc, const char * argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Error: wrong number of arguments: %d.\nUsage: %s diskname\n",
                argc, argv[0]);
        exit(1);
    }

    imgFD = open(argv[1], O_RDONLY);
    if(imgFD < 0) {
        fprintf(stderr, "Cannot open image: %s\n", argv[1]);
        exit(1);
    }
    if(pread(imgFD, &sb, sizeof(struct ext2_super_block), SB) < 0) {
        fprintf(stderr, "Cannot open image: %s\n", argv[1]);
        exit(1);
    }

    blSize = EXT2_MIN_BLOCK_SIZE << sb.s_log_block_size;
    printf("%s,%u,%u,%u,%u,%u,%u,%u\n", "SUPERBLOCK",
           sb.s_blocks_count, sb.s_inodes_count, blSize, sb.s_inode_size,
           sb.s_blocks_per_group, sb.s_inodes_per_group, sb.s_first_ino);

    gdArr = (struct ext2_group_desc*) malloc(groupc * sizeof(struct ext2_group_desc));

    if(gdArr == NULL) {
        fprintf(stderr, "Error: unable to allocate memory for gdArr\n");
        free(gdArr);
        exit(1);
    }

    int i;
    for (i = 0; i < groupc; i++) {
        if (pread(imgFD, &gdArr[i], sizeof(struct ext2_group_desc), getGroupOffset(i)) < 0)
            fprintf(stderr, "Error: cannot read group descriptors\n");
        groupPrint(i); 
    }
    
    for (i = 0; i < groupc; i++) {
        procBm(i);
    }
    for (i = 0; i < groupc; i++) {
        procInbm(i);
    }
    for (i = 0; i < groupc; i++) {
        procIn(i);
    }

    free(gdArr);
    exit(0);
}
