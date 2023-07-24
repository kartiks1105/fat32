#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "helper.h"
#include "fat32.h"


//to find the first sector in a given cluster
uint32_t getFirstSec(fat32Head *h, uint32_t a)
{
    uint32_t firstSec = ((a-2)* h->bs->BPB_SecPerClus) + h->firstDataSector;
    return firstSec;
}


//to read content of the FAT Table
uint32_t readEntry(fat32Head *h, uint32_t b)
{
    uint32_t sectorNum = h->bs->BPB_RsvdSecCnt + ((b*4)/h->bs->BPB_BytesPerSec);
    uint32_t offSetEntry = (b*4) % h -> bs -> BPB_BytesPerSec;

    off_t offset = lseek(h -> fd, (sectorNum * (uint32_t)h -> bs -> BPB_BytesPerSec), SEEK_SET);
    if(offset == -1)
    {
        perror("ERROR!");
        return 0; 
    }

    uint8_t sectorBuff[h -> bs -> BPB_BytesPerSec];
    int readBytes = read(h -> fd, sectorBuff, h -> bs -> BPB_BytesPerSec);

    if(readBytes == -1)
    {
        perror("Could not read fat entry");
        return 0;
    }

    return (*((uint32_t *)&sectorBuff[offSetEntry])) & 0x0FFFFFFF;
}


int nameOfFileAndExtension(char *ch, char ch1[9], int len)
{
    int i = 0;

    while(i < len && ch[i] != ' ')
    {
        ch1[i] = ch[i];
        i++;
    }
    ch1[i] = '\0';

    return 1;
}


int readClus(fat32Head *h, uint32_t clusBegin, uint8_t *buf, uint32_t size)
{
    uint32_t first = getFirstSec(h, clusBegin);

    off_t offSet = lseek(h -> fd, first * (uint32_t)h -> bs -> BPB_BytesPerSec, SEEK_SET);
    if(offSet == -1)
    {
        perror("ERROR!");
        return 0;
    }

    int readBytes = read(h -> fd, buf, size);
    if(readBytes == -1)
    {
        perror("Could not read bytes");
        return 0;
    }

    return 1;
}


bool checker(uint8_t file, uint8_t attr)
{
    if ((file & attr) == attr)
    {
        return true;
    }
    else
    {
        return false;
    }
}