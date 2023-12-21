#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "fs.h"

#define BLOCK_SIZE (BSIZE)

char *addr;
struct superblock* sb;              // Superblock
int bitblocks;                      // Number of bits per block
int* dataBlockAddresses;            // Array to hold data block usage (1 or 0)
int* bitmapArr;                     // Array to hold bitmap
struct dinode *inodeArr;            // Array to hold inodes
int* refCount;                      // array stores file/directory ref count by inode num (test 11 and 12)

int numBitmapBlocks;                // Number of block for the bitmap
int bitmapStartingBlock;            // Starting block for bitmap
int bitmapEndingBlock;              // Ending block for bitmap

int numInodeBlocks;                 // Number of inode blocks
int inodeStartingBlock = 2;         // Inode starting block
int inodeEndingBlock;               // Inode ending block

int dataStartingBlock;              // Starting block number of data blocks
int dataEndingBlock;                // Ending block number of data blocks

int* directoryBuiltInodeBitmap;     // Inode bitmap build while traversing the directory


// List of error messages
const char* errTest1 =           "ERROR: bad inode.\n";
const char* errTest2_direct =    "ERROR: bad direct address in inode.\n";
const char* errTest2_indirect =  "ERROR: bad indirect address in inode.\n";
const char* errTest3 =           "ERROR: root directory does not exist.\n";
const char* errTest4 =           "ERROR: directory not properly formatted.\n";
const char* errTest5 =           "ERROR: address used by inode but marked free in bitmap.\n";
const char* errTest6 =           "ERROR: bitmap marks block in use but it is not in use.\n";
const char* errTest7 =           "ERROR: direct address used more than once.\n";
const char* errTest8 =           "ERROR: indirect address used more than once.\n";
const char* errTest9 =           "ERROR: inode marked use but not found in a directory.\n";
const char* errTest10 =          "ERROR: inode referred to in directory but marked free.\n";
const char* errTest11=           "ERROR: bad reference count for file.\n";
const char* errTest12 =          "ERROR: directory appears more than once in file system.\n";


// List of function declarations
void buildMemBitmap(int);
void buildInodeArr(int);
int* buildInodeBitmap(int);
void errFound(const char*);
void directoryTreeTraverse(struct dirent*);
void test1(struct dinode*);
void test3(struct dirent*);
void test4(struct dirent*);
void test278(struct dinode*, int, int);
void test56(int);
void test910();
void test11();



int
main(int argc, char *argv[])
{
  int i,n,fsfd;
  struct dinode *dip;
  struct dirent *de;
  struct dirent *root_1st_de_ptr;

  if(argc < 2){
    fprintf(stderr, "Usage: fcheck <file_system_image>\n");
    exit(1);
  }


  fsfd = open(argv[1], O_RDONLY);
  if(fsfd < 0){
    fprintf(stderr, "image not found.\n");
    perror(argv[1]);
    exit(1);
  }

  /* Dont hard code the size of file. Use fstat to get the size */
  struct stat fileStat;                       // Struct to hold file stats
  int status = fstat(fsfd, &fileStat);        // Get file stats for fsfd
  assert(status == 0);                        // Ensure there was no error
  int fsfdSize = fileStat.st_size;            // Save file size
  
  addr = mmap(NULL, fsfdSize, PROT_READ, MAP_PRIVATE, fsfd, 0);
  if (addr == MAP_FAILED){
	  perror("mmap failed");
	  exit(1);
  }
  
  /* read the super block */
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);
  // printf("fs size %d, no. of blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);


  // Calculate number of inode blocks
  numInodeBlocks = sb->ninodes / IPB;
  inodeEndingBlock = numInodeBlocks + inodeStartingBlock;
  // printf("numInodeBlocks: %d\n", numInodeBlocks);
  // printf("inodeEndingBlock: %d\n", inodeEndingBlock);

  // Create arrays for data addresses, bitmap, and inode
  dataBlockAddresses = (int*) calloc(sb->nblocks, sizeof(int));
  bitmapArr = (int*) calloc(sb->size, sizeof(int));
  inodeArr = (struct dinode*) calloc(sb->ninodes + 1, sizeof(struct dinode));
  directoryBuiltInodeBitmap = (int*) calloc(sb->ninodes, sizeof(int));
  refCount = (int*) calloc(sb->ninodes+1,sizeof(int));

  // Calculate bitmap starting and ending blocks
  bitmapStartingBlock = BBLOCK(0, sb->ninodes);
  bitmapEndingBlock = BBLOCK(sb->nblocks, sb->ninodes);
  numBitmapBlocks = bitmapEndingBlock - bitmapStartingBlock + 1;

  // Calculate starting and ending block numbers for data blocks
  dataStartingBlock = bitmapEndingBlock + 1;
  dataEndingBlock = dataStartingBlock + sb->nblocks;
  // printf("dataStartingBlock: %d\n", dataStartingBlock);
  // printf("dataEndingBlock: %d\n", dataEndingBlock);

  // Build bitmap array
  buildMemBitmap(fsfd);

  // Build inode array
  buildInodeArr(fsfd);

  // FOR TESTING PURPOSES /* print inode size, inodes per block */
  bitblocks = (int)(sb->size/(BLOCK_SIZE*8)+1);
  // printf("Inode size: %d, inodes per block: %d, bit blocks: %d, used meta blocks: %d\n", (int)sizeof(struct dinode), (int)IPB, bitblocks, (int)(sb->ninodes / IPB + 3 + bitblocks));



  /* read the inodes */
  dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE); 
  // printf("begin addr %p, begin inode %p , offset %ld \n", addr, dip, (char *)dip -addr);

  // read root inode
  // printf("Root inode  size %d links %d type %d \n", dip[ROOTINO].size, dip[ROOTINO].nlink, dip[ROOTINO].type);

  // get the address of root dir 
  de = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE);
  root_1st_de_ptr = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE); //JA: initialized to the same value, but this one won't change

  // print the entries in the first block of root dir 
  n = dip[ROOTINO].size/sizeof(struct dirent);
  for (i = 0; i < n; i++,de++){
 	  // printf(" inum %d, name %s ", de->inum, de->name);
  	// printf("inode  size %d links %d type %d \n", dip[de->inum].size, dip[de->inum].nlink, dip[de->inum].type);
  }


  // ================================================================================================================================

  // Test 1, 2, 7, 8
  int counter = 0;
  for(; counter < sb->ninodes; counter++) {
    // printf("inodeArr[%d]: %d\n", counter, inodeArr[counter].type);
    test1(&(inodeArr[counter]));
    test278(&(inodeArr[counter]), sb->size, fsfd);
  }
  
  // Test 5 and 6
  test56(fsfd);

  // Test 3
  test3(root_1st_de_ptr);

  // Traverse the directory tree
  // Test 4 and 12 is called with in this function
  directoryBuiltInodeBitmap[ROOTINO] = 1;     // Since root inode is always used
  directoryTreeTraverse(root_1st_de_ptr);     // Test 4 and 12 are inside the directory traverse


  // Test 11
  test11();

  // Test 9 and 10
  test910();


  // printf("All tests passed\n");
  exit(0);
}

/* DIRECTORY TRAVERSAL
// primarily written by Justin Aegtema jtb200002
// should eventually include at least tests 4, 9, 10, 11, 12
*/
void
directoryTreeTraverse(struct dirent* directoryEntry1) {
  // printf("Direc Tree Traversal NEW PARENT: inum %d, name %s\n", directoryEntry1->inum, directoryEntry1->name);
  int i;
  int numDirectoryEntries1;
  numDirectoryEntries1 = inodeArr[directoryEntry1->inum].size/sizeof(struct dirent);

  // printf("numDirectoryEntries1: %d\n", numDirectoryEntries1); 

  test4(directoryEntry1);
  directoryEntry1 = directoryEntry1 + 2;
  for (i = 0; i < numDirectoryEntries1-2; i++,directoryEntry1++)
  {
    if(directoryEntry1->inum == 0){
      continue;
    }

    // printf("[%d]      Direc Tree Traversal CHILD inum %d, name %s ",i , directoryEntry1->inum, directoryEntry1->name);
    // printf("        | type: %d\n", inodeArr[directoryEntry1->inum].type);


    // This fixes the block 111 problem, but causes code to flunk dironce
    //if(directoryEntry1->name[0] == '\0'){
    //  continue;
    //}
    // JA: I never used this but w some tweaks it might be handy//printf("DIREC TREE TRAVERSAL inode  size %d links %d type %d \n", dip[de->inum].size, dip[de->inum].nlink, dip[de->inum].type);
    
    // printf("refCount inum:%d\n",directoryEntry1->inum);
    refCount[directoryEntry1->inum]++;

    directoryBuiltInodeBitmap[directoryEntry1->inum] = 1;

    if(inodeArr[directoryEntry1->inum].type == 1) // check if this directory entry it itself a directory
    {

      // checkingCounter++;
      // if(checkingCounter>4){
      //   printf("\nCHECKING COUNTER > 4\n");
      //   exit(1);
      // }

      // Test 12
      if(refCount[directoryEntry1->inum]>1){
        errFound(errTest12);
      }

      directoryTreeTraverse((struct dirent*)(addr + inodeArr[(directoryEntry1->inum)].addrs[0]*BLOCK_SIZE));
    }
  }
  
  // printf("     [DONE EXPLORING]   Direc Tree Traversal CHILD inum %d, name %s \n", directoryEntry1->inum, directoryEntry1->name);
}


// Build inode array
void
buildInodeArr(int fsfd) {
  int inodeCounter = 1;                                                         // Counter number of inodes
  int offset = (inodeStartingBlock * BLOCK_SIZE) + sizeof(struct dinode);       // Offset for first inode from beginning of FS image, also skip inode[0]
  
  // printf("offset: %d\n", offset);
  // printf("sizeof(struct dinode): %d\n", (int) sizeof(struct dinode));

  // Move cursor
  lseek(fsfd, offset, SEEK_SET);


  // Loop through all inodes
  for(; inodeCounter < sb->ninodes + 1; inodeCounter++) {

    // Read inode directly into inodeArr
    read(fsfd, &(inodeArr[inodeCounter]), sizeof(struct dinode));
    
    // FOR TESTING ================================================================
    // printf("=============================================\n");
    // printf("inodeArr[%d]: %p\n", inodeCounter, &(inodeArr[inodeCounter]));
    // printf("inodeArr[%d]: size: %d\n", inodeCounter, inodeArr[inodeCounter].size);
    // printf("inodeArr[%d]: links: %d\n", inodeCounter, inodeArr[inodeCounter].nlink);
    // printf("inodeArr[%d]: type: %d\n", inodeCounter, inodeArr[inodeCounter].type);
    // printf("=============================================\n");
    // if(inodeCounter == 15) break;
  }

}

// Build bitmap array
void
buildMemBitmap(int fsfd) {
  int totBitCounter = 0;                                  // bit counter
  int bmapStartByte = bitmapStartingBlock * BLOCK_SIZE;   // starting position for reading

  lseek(fsfd, bmapStartByte, SEEK_SET);   // Move cursor to starting position

  int bit;    // Save bit
  int byte;   // Save byte

  int a;    // Loop variables
  int b;    // Loop variables

  for(a = 0; a < sb->size; a += 8) {
    // Read first byte
    read(fsfd, &byte, 1);

    // Loop through byte, extracting a new bit each iteration
    for(b = 0; b < 8; b++) {
      bit = (byte >> b) % 2;    // Get bit
      bitmapArr[totBitCounter++] = bit;   // Save bit

      // printf("%d-%d   ",totBitCounter, bit);
      // printf("%d ", bit);
    }
  }

  // printf("totBitCounter: %d\n", totBitCounter);
  // printf("bitmapStartingBlock: %d\n", bitmapStartingBlock);
  // printf("bitmapEndingBlock: %d\n", bitmapEndingBlock);

  // printf("bitmap: ");
  // int q = 0;
  // for (; q < totBitCounter; q++)
  //   printf("  %d", bitmapArr[q]);
  // printf("\n");

}

// Build bitmap based on data blocks in inodes
int*
buildInodeBitmap(int fsfd) {
  int a;      // Loop variables
  int i;      // Loop variables
  int maxBound = dataEndingBlock;     // Boundaries for valid block numbers
  int minBound = dataStartingBlock;   // Boundaries for valid block numbers

  int* inodeBuiltBitmap = (int*) calloc(sb->size, sizeof(int));
  uint indirectBlockNum;                    // Save the indirect data block number

  // Loop through inodes
  for(a = 0; a < sb->ninodes; a++) {
    struct dinode* inode = &(inodeArr[a]);

    // Skip unused inodes
    if(inode->type == 0) continue;

    // Build inodeBuiltBitmap from the inodes
    // Direct block section
    for(i = 0; i < NDIRECT + 1; i++) {

      // Skip empty blocks and out-of-bounds blocks
      if(inode->addrs[i] == 0) continue;
      if(inode->addrs[i] > maxBound || inode->addrs[i] < minBound) continue;

      // Mark valid blocks
      inodeBuiltBitmap[inode->addrs[i]] = 1;

    }

    // Indirect section
    // if(inode->addrs[NDIRECT] == 0) continue;     // Skip empty indirect tables

    // Get indirect block
    lseek(fsfd, inode->addrs[NDIRECT] * BLOCK_SIZE, SEEK_SET);
    for(i = 0; i < NINDIRECT; i++) {
      // Read first indirect block number
      read(fsfd, &indirectBlockNum, sizeof(uint));

      // Skip empty blocks and out-of-bounds blocks
      if(indirectBlockNum == 0) continue;
      if(indirectBlockNum > maxBound || indirectBlockNum < minBound) continue;

      // printf("[%d] indirectBlockNum: %d\n", i, indirectBlockNum);

      // Mark valid block
      inodeBuiltBitmap[indirectBlockNum] = 1;
    }
  }


  // Mark blocks used for inodes, superblock, and bitmap itself
  for(i = 0; i <= bitmapEndingBlock; i++) {
    inodeBuiltBitmap[i] = 1;
  }

  // printf("inodeBuiltMap\n");
  // for(i = 0; i < sb->nblocks; i++) {
    // if (inodeBuiltBitmap[i] == 0) continue;
    // printf("%d-%d   ", i, inodeBuiltBitmap[i]);
    // if (i % 8 == 0) printf("\n");
  // }
  // printf("inodeBuiltMap\n");

  return inodeBuiltBitmap;
}

// Print error and exit
void
errFound(const char* errMessage) {
  fprintf(stderr, errMessage);
  exit(1);
}

// Test 1 - Test for valid inode types
void 
test1(struct dinode* inode) {
  if(inode->type == 0) return;

  int type = inode->type;

  if(type > 3 || type < 1)
    errFound(errTest1);
}

// Test 3 - Test for valid root inode
// coded by both Ryan Vera and Justin Aegtema jtb200002
void test3(struct dirent* root_1st_de_ptr) {
 
  // Check if root directory exists
  if(root_1st_de_ptr == NULL) {
    errFound(errTest3);
  }

  // Not JA: Check if root is a directory // JA: I don't think we need anything here

  // Check that the inode number is 1
  if(root_1st_de_ptr->inum != 1) {
    errFound(errTest3);
  }

  // Check that its parent is itself
  //char* root_parent;
  //root_parent = malloc(strlen((root_1st_de_ptr+1)->name) + 1);
  //root_parent = (root_1st_de_ptr+1)->name;
  //char exp_root_parent[] = "..";
  //if(strcmp((root_1st_de_ptr+1)->name,exp_root_parent) != 0) {
  //  errFound(errTest3);
  //}

  // Check that root director parent is itself
  if((root_1st_de_ptr+1)->inum != 1) {
    errFound(errTest3);
  }

  //free(root_parent);

}

// Test 4
// written by Justin Aegtema jtb200002
// check for "." and ".." self and parent naming in 1st two directory entries
// check that "." first directory entry points to self
void test4(struct dirent* directoryEntry1){
  char expectedNameSelf[] = ".";
  char expectedNameParent[] = "..";

  //   printf("Checking: directory entry inum: %d\n",directoryEntry1->inum );
  //  struct dinode *dip1;
  if(strcmp((directoryEntry1)->name,expectedNameSelf) != 0) {
    // printf("CASE 1 ");
    errFound(errTest4);
  }
  //   printf("CASE 1 PASSED ");
  //   printf("Should be '.' - '%s'",(directoryEntry1)->name);

  if(strcmp((directoryEntry1+1)->name,expectedNameParent) != 0) {
    // printf("CASE 2 ");
    errFound(errTest4);
  }
  //   printf("CASE 2 PASSED ");
  //   printf("Should be '..' - '%s'",(directoryEntry1+1)->name);

  if(directoryEntry1 != (struct dirent *)(addr + (inodeArr[(directoryEntry1)->inum].addrs[0])*BLOCK_SIZE)) {
    // printf("CASE 3 ");
    errFound(errTest4);
  }

  //   printf("CASE 3 PASSED \n");
}

// Test 2, 7 8 - Test for bad direct and indirect address in an inode
void 
test278(struct dinode *inode, int size, int fsfd) {
  int i = 0;
  int maxBound = dataEndingBlock - 1;     // Boundaries for valid block numbers
  int minBound = dataStartingBlock;     // Boundaries for valid block numbers

  uint* directSection = inode->addrs;       // Save direct block addresses
  uint indirectBlockNum;                    // Save the indirect data block number

  // Skip unused inodes
  if(inode->type == 0) return;

  // Get indirect block
  lseek(fsfd, inode->addrs[NDIRECT] * BLOCK_SIZE, SEEK_SET);

  // Test 2 and 7
  // Loop through direct section
  for(i = 0; i < NDIRECT; i++) {

    // Skip empty blocks
    if(directSection[i] == 0) continue;


    // Test 2 =================================================================
    // Skip numbers that aren't in the correct bounds
    if(directSection[i] > maxBound || directSection[i] < minBound) {
      // printf("directSection[%d]: %d\n", i, directSection[i]);
      errFound(errTest2_direct);
    }

    // Test 7 =================================================================
    // Error on blocks that are already marked, if not marked -> mark them
    if(dataBlockAddresses[directSection[i]] == 0) {
      dataBlockAddresses[directSection[i]] = 1;
      // printf("dataBlockAddresses[directSection[%d]]: %d\n", i, directSection[i]);
    }
    else {
      // printf("[FAILED]   dataBlockAddresses[directSection[%d]]: %d\n", i, directSection[i]);
      errFound(errTest7);
    }

  }


  // Test 2 and 8
  // Loop through indirect section
  for(i = 0; i < NINDIRECT; i++) {
    // Read first indirect block number
    read(fsfd, &indirectBlockNum, sizeof(uint));

    // Skip empty blocks
    if(indirectBlockNum == 0) continue;

    // Test 2 =================================================================
    // Skip numbers that aren't in the correct bounds
    if(indirectBlockNum > maxBound || indirectBlockNum < minBound) {
      // printf("indirectBlockNum: %d\n", indirectBlockNum);
      errFound(errTest2_indirect);
    }


    // Test 8 =================================================================
    // Error on blocks that are alread marked, if not marked -> mark them
    if(dataBlockAddresses[indirectBlockNum] == 0) {
      dataBlockAddresses[indirectBlockNum] = 1;
      // printf("dataBlockAddresses[%d]: %d\n", indirectBlockNum, dataBlockAddresses[indirectBlockNum]);
    }
    else {
      // printf("[FAILED]   dataBlockAddresses[%d]: %d\n", indirectBlockNum, dataBlockAddresses[indirectBlockNum]);
      errFound(errTest8);
    }

  }

}

// Test 5 and 6 - Test for blocks in inode are marked properly in the bitmap
void
test56(int fsfd) {
  int i;

  // Build a bitmap based off the data blocks that are in the inodes
  int* inodeBuiltBitmap = buildInodeBitmap(fsfd);

  // Compare true bitmap and inodeBuiltBitmap
  for(i = 0; i < sb->nblocks; i++) {

    // Skip indices where both lists are unmarked
    if(bitmapArr[i] == inodeBuiltBitmap[i]) {
      // printf("They are consistent\n");
      continue;
    }

    // printf("bitmapArr[%d]: %d | inodeBuiltBitmap[%d]: %d\n", i, bitmapArr[i], i, inodeBuiltBitmap[i]);

    // Test 5 - marked used by inode and not bitmap
    if(bitmapArr[i] == 0 && inodeBuiltBitmap[i] == 1) {
      free(inodeBuiltBitmap);   // Free memory
      errFound(errTest5);       // Report error
    }

    // Test 6 = marked used by bitmap and not by inode
    else if(bitmapArr[i] == 1 && inodeBuiltBitmap[i] == 0) {
      free(inodeBuiltBitmap);  // Free memory
      errFound(errTest6);      // Report error
    }

  }

  free(inodeBuiltBitmap);   // Free memory
}


// Test 9 and 10 - Test for consistence between inodes and indodes in the directory
void
test910() {
  int i;

  for(i = 0; i < sb->ninodes; i++) {

    int trueInodeMarking = inodeArr[i].type < 4 && inodeArr[i].type > 0 ? 1 : 0;
    int directoryInodeMarking = directoryBuiltInodeBitmap[i];

    assert(trueInodeMarking == 1 || trueInodeMarking == 0);   // Ensure that the inode type gets converted to a marking correctly
    // printf("[%d]   trueInodeMarking: %d   directoryInodeMarking: %d", i, trueInodeMarking, directoryInodeMarking);
    // printf(trueInodeMarking == directoryInodeMarking ? "      \n" : "   +++\n");


    // Skip if elements are the same
    if(trueInodeMarking == directoryInodeMarking) {
      // printf("They are consistent\n");
      continue;
    }

    // Test 9 - marked in bitmap but not found in directory
    if(trueInodeMarking == 1 && directoryInodeMarking == 0) {
      free(directoryBuiltInodeBitmap);
      errFound(errTest9);
    }


    // Test 10 - found in directory but unmarked in bitmap
    else if(trueInodeMarking == 0 && directoryInodeMarking == 1) {
      free(directoryBuiltInodeBitmap);
      errFound(errTest10);
    }

  }

  free(directoryBuiltInodeBitmap);
}


// Test 11 - Test whether actual reference counts matches inode records
// function written by Justin Aegtema jtb200002
void
test11(){
  int i;
  for(i=1; i <= sb->ninodes; i++){
    if(inodeArr[i].type == 2){ //check to see if it's an ordinary file
      // printf("i: %d ;; inodeArr[i].type: %d ;; refCount[i]: %d ;; inodeArr[i].nlink: %d \n", i, inodeArr[i].type, refCount[i], inodeArr[i].nlink);
      if(refCount[i] != inodeArr[i].nlink) {
        errFound(errTest11);
      }
    }
  }
}