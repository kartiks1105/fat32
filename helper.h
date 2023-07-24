#ifndef HELPER_H
#define HELPER_H
#include <stdbool.h>
#include "fat32.h"
uint32_t readEntry(fat32Head *h, uint32_t a);
int nameOfFileAndExtension(char *ch, char *str, int len);
bool checker(uint8_t fileAttr, uint8_t attr);
uint32_t getFirstSec(fat32Head *h, uint32_t b);
int readClus(fat32Head *h, uint32_t clusBegin, uint8_t *buf, uint32_t size);

#endif  