// Quick and dirty tool to pack data into a ColecoVision Megacart
//
// packcolecocart <outfile> <program> <data1> <data2> <data3> ...
// 
// Program first ensures that the program is padded to 8k (warn if bigger).
// 
// For each datafile in the list:
// -Report the address and the bank switch for it
// -for each 8k page, copy 16384-256 bytes of the data (to avoid Megacart switching area)
// -pad the last block of each file to 16k, no sharing
// -copy the program into the last page
// -after the last file, report the next free page
//
// For now, assumes your program is 16k or less
//

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define PAGESIZE 16384
#define SKIPSIZE 256

unsigned char buf[PAGESIZE];
FILE *fIn, *fOut;

// got to stuff my credits somewhere
const unsigned char *EGOSTRING = (const unsigned char *)"Made with packcolecocart by Tursi - http://harmlesslion.com - ";

void egofill(unsigned char *buf, const unsigned char *datastr, int size) {
	const unsigned char *pStr = datastr;
	while (size--) {
		*(buf++)=*(pStr++);
		if (*pStr == '\0') pStr = datastr;
	}
}

int main(int argc, char *argv[]) {
	int bank;
	int adr;

	if (argc < 4) {
		printf("packcolecocart <outfile> <program> <data1> <data2> <data3> ...\n");
		printf("Program is padded to 16k, then banks are appended.\n");
		printf("MegaCart uses inverted bank order, program goes at the end.\n");
		return -1;
	}

	fOut = fopen(argv[1], "wb");
	if (NULL == fOut) {
		printf("Failed to open outfile, code %d\n", errno);
		return 1;
	}

	// since we have to work backwards, we first size the programs
	int totalSize = 0;
	for (int idx=2; idx<argc; idx++) {
		fIn = fopen(argv[idx], "rb");
		if (NULL == fIn) {
			printf("Failed to size file %s, code %d\n", argv[idx], errno);
			return 4;
		}
		fseek(fIn, 0, SEEK_END);
		int filesize = ftell(fIn);
		fclose(fIn);

		// now we know the number in bytes, we figure out how many pages that is
		totalSize += (filesize + (PAGESIZE-SKIPSIZE-1)) / (PAGESIZE-SKIPSIZE);
	}
	
	// totalSize now has number of pages that the files will take
	// so we need to figure out the padding (power of 2)
	int neededSize = 1;
	while (neededSize < totalSize) {
		neededSize *= 2;
	}

	bank = 0x10000 - neededSize;
	adr = 0;

	if (neededSize > totalSize) {
		printf("Padding for %dk\n", neededSize*16);
		if (neededSize > 64) printf("* Warning - 1MB is max MegaCart so far.\n");

		egofill(buf, EGOSTRING, PAGESIZE);
		for (int idx = totalSize; idx < neededSize; idx++) {
			fwrite(buf, 1, PAGESIZE, fOut);
			++bank;
			adr+=PAGESIZE;
		}
	}

	// now loop the data files
	for (int idx = 3; idx<argc; idx++) {
		fIn = fopen(argv[idx], "rb");
		if (NULL == fIn) {
			printf("Failed to open data file %s, code %d\n", argv[idx], errno);
			return 3;
		}

		printf("Opened data file '%s'\n", argv[idx]);
		printf("- starting address %X (page >%04X)\n", adr, bank);

		while (!feof(fIn)) {
			memset(buf, 0, PAGESIZE);
			if (0 == fread(buf, 1, PAGESIZE-SKIPSIZE, fIn)) break;
			fwrite(buf, 1, PAGESIZE, fOut);
			++bank;
			adr+=PAGESIZE;
		}

		fclose(fIn);
		printf("- ending address %X (page >%04X)\n", adr, bank);
	}

	// get the program 
	fIn = fopen(argv[2], "rb");
	if (NULL == fIn) {
		printf("Failed to open program file, code %d\n", errno);
		return 2;
	}
	memset(buf, 0, PAGESIZE);
	fread(buf, 1, PAGESIZE, fIn);

	fgetc(fIn);
	if (!feof(fIn)) {
		printf("Warning - program file may be too large - only %d bytes read!\n", PAGESIZE);
	}
	fclose(fIn);
	
	// save the last page
	fwrite(buf, 1, PAGESIZE, fOut);

	printf("Opened program file '%s'\n", argv[2]);
	printf("- starting address %X (page >%04X)\n", adr, bank);

	if (bank != 0xffff) {
		printf("* WARNING - program should be at 0xffff - internal error?\n");
	}

	fclose(fOut);
	printf("Finished writing '%s'.\n", argv[1]);

	return 0;
}
