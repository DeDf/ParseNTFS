#ifndef _H_GLOBLE_
#define _H_GLOBLE_

#include <windows.h>
#include <stdio.h>
#include <locale.h>
#include "ntfs.h"

#pragma pack(1)
typedef struct _PARTITION_ENTRY {
    unsigned char boot_ind;		/* 分区标志 0x80 - active */
    unsigned char head;			/* 开始磁头 starting head */
    unsigned short sector:6;	/* 开始扇区 starting sector */
    unsigned short cyl:10;		/* 开始柱面 starting cylinder */
    unsigned char sys_ind;	    /* 系统ID   What partition type */
    unsigned char end_head;		/* 结束磁头 end head */
    unsigned char end_sector;	/* 结束扇区 end sector */
    unsigned char end_cyl;		/* 结束柱面 end cylinder */
    unsigned int start_sect;	/* 相对扇区数 starting sector counting from 0 */
    unsigned int nr_sects;		/* 总扇区数 sum of sectors in this partition */
}PARTITION_ENTRY,* PPARTITION_ENTRY;
#pragma pack()

void AnalysePartition();

#endif