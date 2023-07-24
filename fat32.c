#include "fat32.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "helper.h"

#define FILE_OFFSET_BITS 64


fat32Head* createHead(int fd)
{
    fat32BS *bS = (fat32BS *)malloc(sizeof(fat32BS));

    //reading bpb and bs
    read(fd, bS, sizeof(fat32BS));
    
    //checking signature bytes
    if(bS -> BS_BootSig != 0x29 || bS -> BS_SigA != 0x55 || bS -> BS_SigB != 0xAA)
    {
        free(bS);
        return NULL;
    }

    fat32Info *fsInfo_ = read_FS_info(fd, bS);
    if(fsInfo_ == NULL)
    {
        free(bS);
        return NULL;
    }

    fat32Head *head = (fat32Head *)malloc(sizeof(fat32Head));
    head -> fd = fd;
    head -> bs = bS;
    head -> fsInfo_ = fsInfo_;

    uint32_t rootDirectorySec = ((head -> bs -> BPB_RootEntCnt * 32) + (head -> bs -> BPB_BytesPerSec - 1)) / (head -> bs -> BPB_BytesPerSec);

    
    uint32_t fatSz;
    if(head -> bs -> BPB_FATSz16 != 0)
    {
        fatSz = head -> bs -> BPB_FATSz16;
    }
    else
    {
        fatSz = (uint32_t)head -> bs -> BPB_FATSz32;
    }
    head -> firstDataSector = (head -> bs -> BPB_RsvdSecCnt) + (head -> bs -> BPB_NumFATs * fatSz) + rootDirectorySec;


    uint32_t numSectors;
    if(head -> bs -> BPB_TotSec16 != 0)
    {
        numSectors = head -> bs -> BPB_TotSec16;
    }
    else
    {
        numSectors = (uint32_t)head -> bs -> BPB_TotSec32;
    }
    
    uint32_t numDataSec = numSectors - (head -> bs -> BPB_RsvdSecCnt + (head -> bs -> BPB_NumFATs * fatSz) + rootDirectorySec);
    head -> numClusters =  numDataSec / head -> bs -> BPB_SecPerClus; //number of clusters


    //volume ID
    uint32_t rootDirectoryClus = head -> bs -> BPB_RootClus;
    uint32_t clusterSize = (uint32_t)head -> bs -> BPB_BytesPerSec * (uint32_t)head -> bs -> BPB_SecPerClus;

    uint8_t clusBuffer[clusterSize];
    if(!readClus(head, rootDirectoryClus, clusBuffer, clusterSize))
    {
        head -> volID = (char *)malloc(sizeof(char));
        strcpy(head->volID, "\0");
    }
    else
    {
        fat32Dir *rootDirectory = (fat32Dir *)(&clusBuffer[0]);
        head -> volID = (char *)malloc(strlen(rootDirectory -> DIR_Name)+1);
        strcpy(head -> volID, rootDirectory -> DIR_Name);
    }

    uint32_t sig = 0X0FFFFF00 | (uint32_t)head -> bs -> BPB_Media;
    if(readEntry(head, 0) != sig || readEntry(head, 1) != 0x0FFFFFFF)
    {
        cleanHead(head);
        return NULL;
    }

    return head;
}


fat32Info *read_FS_info(int fd, fat32BS *bs)
{
    fat32Info *fsInfo_ = (fat32Info *)malloc(sizeof(fat32Info));
    
    //FSInfo structure
    if(lseek(fd, (bs -> BPB_FSInfo * bs -> BPB_BytesPerSec), SEEK_SET) == -1)
    {
        perror("Error!");
        free(fsInfo_);
        return NULL;
    }

    read(fd, fsInfo_, sizeof(fat32Info));

    return fsInfo_;

}


int cleanHead(fat32Head *h)
{
    if(h == NULL)
    {
        return 0;
    }
    else
    {
        free(h -> volID);
        free(h -> bs);
        free(h -> fsInfo_);
        free(h);
    }

    return 0;
}