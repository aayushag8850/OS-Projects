#include "mfs.h"
#include "udp.h"
#include "message.h"
#include <sys/select.h>
#include <time.h>
#include <stdio.h>

int sd;
int rc;
struct sockaddr_in req, rcv;
struct timeval tv;
message_t mess; 

void req_rec(){
    rc = UDP_Write(sd, &req, (void*) &mess, sizeof(message_t));
    rc = UDP_Read(sd, &rcv, (void*) &mess, sizeof(message_t));
    return;
}

int MFS_Init(char *hostname, int port) {
    int MIN_PORT = 20000;
    int MAX_PORT = 40000;
    srand(time(0));
    int port_num = (rand() % (MAX_PORT - MIN_PORT) + MIN_PORT); 

    sd = UDP_Open(port_num);
    if (sd < 0) return sd;
    rc = UDP_FillSockAddr(&req, hostname, port);
    if (rc < 0) return rc;
    return 0;
}
/**
 * takes the parent inode number (which should be the inode number of a directory) and looks up the entry name in it.
 * The inode number of name is returned. 
 * Success: return inode number of name; failure: return -1. 
 * Failure modes: invalid pinum, name does not exist in pinum
*/
int MFS_Lookup(int pinum, char *name) {
    mess.mtype = MFS_LOOKUP;
    mess.inum = pinum;
    strcpy(mess.name, name);
    req_rec();
    return mess.rc;
}

/**
 * Returns some information about the file specified by inum. Upon success, return 0, otherwise -1. 
 * The exact info returned is defined by MFS_Stat_t. 
 * Failure modes: inum does not exist. File and directory sizes are described below.
*/
int MFS_Stat(int inum, MFS_Stat_t *m) {
    mess.inum = inum;
    mess.mtype = MFS_STAT;
    req_rec();
    m->size = mess.nbytes;
    m->type = mess.type;
    return mess.rc;
}

/**
 * writes a buffer of size nbytes (max size: 4096 bytes) at the byte offset specified by offset.
 * Returns 0 on success, -1 on failure. 
 * Failure modes: invalid inum, invalid nbytes, invalid offset, 
 * not a regular file (because you can't write to directories).
*/
int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    mess.mtype = MFS_WRITE;
    mess.inum = inum;
    memcpy(mess.buffer, buffer, MFS_BLOCK_SIZE);
    mess.offset = offset;
    mess.nbytes = nbytes;
    req_rec();
    return mess.rc;
}

/**
 * reads nbytes of data (max size 4096 bytes) specified by the byte offset offset into the buffer from file 
 * specified by inum. The routine should work for either a file or directory; 
 * directories should return data in the format specified by MFS_DirEnt_t. 
 * Success: 0, failure: -1. Failure modes: invalid inum, invalid offset, invalid nbytes.
*/
int MFS_Read(int inum, char *buffer, int offset, int nbytes) {
    mess.mtype = MFS_READ;
    mess.inum = inum;
    mess.offset = offset;
    mess.nbytes = nbytes;
    req_rec(); 
    memcpy(buffer, mess.buffer, nbytes);
    return mess.rc;
}

/**
 * makes a file (type == MFS_REGULAR_FILE) or directory (type == MFS_DIRECTORY) in the parent directory specified by pinum
 * of name name. Returns 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, or name is too long. If name already exists, return success.
*/
int MFS_Creat(int pinum, int type, char *name) {
    mess.mtype = MFS_CREAT;
    mess.inum = pinum;
    mess.type = type;
    strcpy(mess.name, name);
    req_rec(); 
    return mess.rc;
}

/**
 * removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure. 
 * Failure modes: pinum does not exist, directory is NOT empty. 
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
*/
int MFS_Unlink(int pinum, char *name) {
    mess.mtype = MFS_UNLINK;
    mess.inum = pinum;
    strcpy(mess.name, name);
    req_rec(); 
    return mess.rc;
}

/**
 * just tells the server to force all of its data structures to disk and shutdown by calling exit(0). 
 * This interface will mostly be used for testing purposes.
*/
int MFS_Shutdown() {
    mess.mtype = MFS_SHUTDOWN;
    req_rec(); 
    UDP_Close(sd);
    return mess.rc;
}