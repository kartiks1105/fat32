#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shell.h"
#include "fat32.h"
#include "helper.h"
#include <stdbool.h>

#define BUF_SIZE 256
#define CMD_INFO "INFO"
#define CMD_DIR "DIR"
#define CMD_CD "CD"
#define CMD_GET "GET"
#define CMD_PUT "PUT"


void shellLoop(int fd) 
{
	int running = true;
	uint32_t curDirClus;
	char buffer[BUF_SIZE];
	char bufferRaw[BUF_SIZE];

	//TODO:
	//head here is just like the head of a linked list - an entry
	//into the data structure for Fat32.
	//it can store whatever you want! It will be convenient to pass
	//it around to all the functions. Start by putting the BS/BPB
	//into it as one of the structs it holds.
	fat32Head *h = createHead(fd);

	if (h == NULL)
		running = false;
	else
		curDirClus = h->bs->BPB_RootClus; // valid, grab the root cluster	
		//TODO
	

	while(running) 
	{
		printf(">");
		if (fgets(bufferRaw, BUF_SIZE, stdin) == NULL) 
		{
			running = false;
			continue;
		}
		bufferRaw[strlen(bufferRaw)-1] = '\0'; /* cut new line */
		for (int i=0; i < strlen(bufferRaw)+1; i++)
			buffer[i] = toupper(bufferRaw[i]);
	
		if (strncmp(buffer, CMD_INFO, strlen(CMD_INFO)) == 0)
			printInfo(h);	

		else if (strncmp(buffer, CMD_DIR, strlen(CMD_DIR)) == 0)
			doDir(h, curDirClus);	
	
		else if (strncmp(buffer, CMD_CD, strlen(CMD_CD)) == 0) 
			curDirClus = doCD(h, curDirClus, buffer);

		else if (strncmp(buffer, CMD_GET, strlen(CMD_GET)) == 0) 
			doDownload(h, curDirClus, buffer);
				

		// else if (strncmp(buffer, CMD_PUT, strlen(CMD_PUT)) == 0)
		 	//doUpload(h, curDirClus, buffer, bufferRaw);
		// 	printf("Bonus marks!\n");
		else 
			printf("\nCommand not found\n");
	}
	printf("\nExited...\n");
	
	cleanHead(h);
}


int printInfo(fat32Head *h)
{
	printf("- - - - Device Info - - - -\n");

	//OEM Name
	printf("  OEM Name: %s\n", h->bs->BS_OEMName);

	//Label
	char label[BS_VolLab_LENGTH+1];
	strncpy(label, h -> bs -> BS_VolLab, BS_VolLab_LENGTH);
	label[BS_VolLab_LENGTH] = '\0';
	printf("  Label: %s\n", label);
	
	//File System Type
	char fileSysType[BS_FilSysType_LENGTH+1];
	strncpy(fileSysType, h -> bs -> BS_FilSysType, BS_FilSysType_LENGTH);
	fileSysType[BS_FilSysType_LENGTH] = '\0';
	printf("  File System Type: %s\n", fileSysType);

	//Media Type
	if((h -> bs -> BPB_Media & 0xF8) == 0xF8)
	{
		printf("  Media type: 0x%X (fixed)\n", h -> bs -> BPB_Media);
	}
	else
	{
		printf("  Media Type: 0x%X (not fixed)\n", h -> bs -> BPB_Media);
	}

	
	uint32_t nSec;
	if(h -> bs -> BPB_TotSec16 == 0)
	{
		nSec = h -> bs -> BPB_TotSec32;
	}
	else
	{
		nSec = h -> bs -> BPB_TotSec16;
	}
	uint64_t totBytes = (uint64_t)h -> bs -> BPB_BytesPerSec * (uint64_t)nSec;
	

	printf("  Size: %ld (%.fMB, %.3fGB)\n", totBytes, (double)totBytes/1000000, (double)totBytes/1000000000);
	printf("  Driver Number: %d", h -> bs -> BS_DrvNum);


	if((h -> bs -> BS_DrvNum & 0x80) == 0x80)
	{
		printf(" (hard disk)\n");
	}
	else if((h -> bs -> BS_DrvNum & 0x00) == 0x00)
	{
		printf(" (floppy disk)\n");
	}


	printf("\n- - - Geometry - - - \n");
	printf("  Bytes Per Sector: %d\n", h -> bs -> BPB_BytesPerSec);
	printf("  Sectors Per Cluster: %d\n", h -> bs -> BPB_SecPerClus);
	printf("  Total Sectors: %d\n", nSec);
	printf("  Geom: Sectors per Track: %d\n", h -> bs -> BPB_SecPerTrk);
	printf("  Geom: Heads: %d\n", h -> bs -> BPB_NumHeads);
	printf("  Hidden Sectors: %d\n", h -> bs -> BPB_HiddSec);


	printf("\n- - - FS INFO - - -\n");
	printf("  Volume ID: %s\n", h -> volID);
	printf("  Version: %d.%d\n", h -> bs -> BPB_FSVerHigh, h -> bs -> BPB_FSVerLow);
	printf("  Reserved Sectors: %d\n", h -> bs -> BPB_RsvdSecCnt);
	printf("  Number of FATs: %d\n", h -> bs -> BPB_NumFATs);

	
	uint32_t size_FAT;
	if(h -> bs -> BPB_FATSz16 == 0)
	{
		size_FAT = h -> bs -> BPB_FATSz32;
	}
	else
	{
		size_FAT = h -> bs -> BPB_FATSz16;
	}
	printf("  FAT Size: %d\n", size_FAT);


	uint16_t fatMirror = (h -> bs -> BPB_ExtFlags & 0x80);
	if(fatMirror)
	{
		printf("  Mirrored FAT: %d (no)\n", fatMirror);
	}
	else
	{
		printf("  Mirrored FAT: %d (yes)\n", fatMirror);
	}


	printf("  Boot Sector Backup Sector No.: %d\n", h -> bs -> BPB_BkBootSec);
	printf("\n");

	return 0;

}


int doDir(fat32Head *h, uint32_t curDirClus)
{
	printf("\n DIRECTORY LISTING \n");
	printf(" VOL_ID: %s\n\n", h->volID);

	fat32Dir *directory = NULL;
	char file_name[50];
	char var[5];
	uint32_t sizeClus = (uint32_t)h->bs->BPB_BytesPerSec * (uint32_t)h->bs->BPB_SecPerClus;
	uint8_t clusBuff[sizeClus];

	do
	{
		directory = (fat32Dir *)(&clusBuff[0]);

		if(!readClus(h, curDirClus, clusBuff, sizeClus))
		{
			printf("Couldn't read the cluster %d\n", curDirClus);
			return 0;
		}


		while(directory -> DIR_Name[0] != 0x00 && directory -> DIR_Name[0] != 0xE5)
		{
			nameOfFileAndExtension(directory -> DIR_Name, file_name, 20);
			nameOfFileAndExtension(&directory -> DIR_Name[8], var, 5);
			
			if(checker(directory -> DIR_Attr, ATTR_DIRECTORY))
			{
				printf("<%s>", file_name);
				printf("\t%d\n", directory -> DIR_FileSize);
			}
			else if(!checker(directory -> DIR_Attr, ATTR_HIDDEN) && !checker(directory -> DIR_Attr, ATTR_VOLUME_ID))
			{
				printf("%s.%s", file_name, var);
				printf("\t%d\n", directory -> DIR_FileSize);
			}
			
			directory++;
		}
		curDirClus = readEntry(h, curDirClus);

	} while(curDirClus != 0x0FFFFFFF && curDirClus < 0x0FFFFFF8 && directory != NULL);


	uint64_t freeByte;
	if((h -> fsInfo_ -> FSI_Free_Count != 0xFFFFFFFF) && (h -> fsInfo_ -> FSI_Free_Count <= h -> numClusters))
	{
		freeByte = (uint64_t)h -> fsInfo_ -> FSI_Free_Count* (uint64_t)h->bs->BPB_SecPerClus * (uint64_t)h->bs->BPB_BytesPerSec;
		printf("---Bytes Free: %ld\n", freeByte);
	}
	else
	{
		printf("---Bytes Free: unknown\n");
	}

	printf("---DONE!\n");
	return 1;

}


int doCD(fat32Head *h, uint32_t curDirClus, char *buffer)
{
	fat32Dir *directory = NULL;
	uint32_t dirClus = 0;
	uint32_t another = curDirClus;
	char directoryName[12];
	uint8_t buff[(uint32_t)h-> bs -> BPB_BytesPerSec * (uint32_t)h -> bs ->BPB_SecPerClus];

	do
	{
		if(!readClus(h, another, buff, ((uint32_t)h-> bs -> BPB_BytesPerSec * (uint32_t)h -> bs ->BPB_SecPerClus)))
		{
			printf("Could not read cluster %d\n", another);
			return 0;
		}

		directory = (fat32Dir *)(&buff[0]);

		while(directory -> DIR_Name[0] != 0x00 && directory->DIR_Name[0] != 0xE5)
		{
			if (checker(directory->DIR_Attr, ATTR_DIRECTORY))
			{
				if (!nameOfFileAndExtension(directory->DIR_Name, directoryName, 11))
				{
					printf("There was an issue parsing directory file_name: %s\n", directory->DIR_Name);
					return 0;
				}

				int dirNameLen = strlen(directoryName);
				int cdLen = strlen(CMD_CD);
				char *here = buffer+cdLen +1;

				bool works = strncmp(here, directoryName, dirNameLen) == 0;
				if(works)
				{
					dirClus = ((((uint32_t)directory->DIR_FstClusHI[1]) << 24) | (((uint32_t)directory->DIR_FstClusHI[0]) << 16) | (((uint32_t)directory->DIR_FstClusLO[1]) << 8) | (uint32_t)directory->DIR_FstClusLO[0]) & 0x0FFFFFFF; 

					if(dirClus == 0)
					{
						return h->bs->BPB_RootClus;
					}
					else
					{
						return dirClus;
					}
				}
			}
			directory++;
		}
		another = readEntry(h, another);
	} while (another != 0x0FFFFFFF && another < 0x0FFFFFF8 && directory != NULL);

	printf("Error! folder does not exist\n");
	return curDirClus;
}


int doDownload( fat32Head *h, uint32_t curDirClus, char *buff)
{
	fat32Dir *directory = NULL;
	uint32_t entry = curDirClus;
	char file_name[9];
	char var[4];
	uint32_t clusSize = (uint32_t)h ->bs -> BPB_BytesPerSec * (uint16_t)h-> bs -> BPB_SecPerClus;
	uint8_t cBuf[clusSize];


	do
	{
		directory = (fat32Dir *)(&cBuf[0]);

		if(!readClus(h, entry, cBuf, clusSize))
		{
			printf("Error! Could not read cluster\n");
			return 0;
		}

		while(directory -> DIR_Name[0] != 0x00)
		{
			if(directory -> DIR_Name[0] != 0xE5)
			{
				if(!checker(directory -> DIR_Attr, ATTR_DIRECTORY))
				{

					if(!nameOfFileAndExtension(directory -> DIR_Name, file_name, 8))
					{
						printf("Error! Could not parse the file file_name: %s\n", directory -> DIR_Name);
						return 0;
					}

					if(!nameOfFileAndExtension(&directory -> DIR_Name[8], var, 3))
					{
						printf("Error! %s\n", directory -> DIR_Name);
						return 0;
					}

					char fName[11 +3];
					strcpy(fName, file_name);
					strcat(fName, ".");
					strcat(fName, var);
					if(strncmp(&buff[strlen(CMD_GET)+1], fName, strlen(&buff[strlen(CMD_GET)+1])) ==0)
					{
						int var1 = open(fName, O_CREAT | O_RDWR, 0700);
						if(var1 == -1)
						{
							perror("Could not create new file!\n");
							return 0;
						}

						uint32_t fClus = ((((uint32_t)directory -> DIR_FstClusHI[1]) << 24) | (((uint32_t)directory -> DIR_FstClusHI[0]) << 16) | (((uint32_t)directory -> DIR_FstClusLO[1]) << 8) | (uint32_t)directory -> DIR_FstClusLO[0]) & 0x0FFFFFFF;
						int count = 0;
						uint32_t c1, remBytes =  directory -> DIR_FileSize;
						
						while (fClus != 0x0FFFFFFF && fClus < 0x0FFFFFF8) 
						{
							do 
							{
								c1 = readEntry(h, fClus + count);
								count++;
							} while (c1 == fClus + count && c1 != 0x0FFFFFFF && c1 < 0x0FFFFFF8);

							int size = clusSize * count;
							if (size > remBytes) 
							{
								size = remBytes;
							}

							uint8_t dBuff[size];
							if (!readClus(h, fClus, dBuff, size)) 
							{
								printf("Could not read cluster %d\n", fClus);
								free(dBuff);
								return 0;
							}

							if (write(var1, dBuff, size) == -1) 
							{
								perror("Error! Could not write to file.");
								close(var1);
								free(dBuff);
								return 0;
							}
							count = 0;
							fClus = c1;
							remBytes -= size;
						}

						
						close(var1);
						return 1;
					}	
				}
			}
			directory++;
		}
		entry = readEntry(h, entry);
	} while(entry != 0x0FFFFFFF && entry < 0x0FFFFFF8 && directory != NULL);
	return 0;
}
