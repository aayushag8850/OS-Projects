#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include "udp.h"
#include "mfs.h"
#include "ufs.h"
#include "message.h"

#define BUFFER_SIZE (4096)
#define MAX_NUM_DIR (128)

int sd, fd, stype, sbytes;
void *saddr; // super block addr
void *dbaddr; // data bit addr
void *ibaddr; // inode bit addr
super_t *sup; // super block
inode_t *inodes; // inodes array

void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}


unsigned int get_bit(unsigned int *bitmap, int position) {
    int index = position / 32;
    int offset = 31 - (position % 32);
    return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position) {
    int index = position / 32;
    int offset = 31 - (position % 32);
    bitmap[index] |= 0x1 << offset;
}

int find_bit(void *bitmap, int length){
    for (int i = 0; i<length; i++){
        if(get_bit(bitmap, i) == 0) return i;
    }
    return -1;
}

int server_lookup(int pinum, char *name){
    if (pinum < 0 || pinum > sup->num_inodes) return -1;
    inode_t parent = inodes[pinum];
    if (parent.type != UFS_DIRECTORY) return -1;
    MFS_DirEnt_t *temp = (MFS_DirEnt_t *)(saddr+ parent.direct[0]*UFS_BLOCK_SIZE);
    for (int j = 0; j<MAX_NUM_DIR; j++){
        if(strcmp(name, temp[j].name) == 0) {return temp[j].inum;}
    }
    return -1;
}

int server_stat(int inum){
    if(inum > sup->num_inodes || inum<0) return -1;
    stype = inodes[inum].type;
    sbytes = inodes[inum].size;
    return 0;
}

int server_write(int inum, char *buffer, int offset, int nbytes){
    if (inum<0 || nbytes<0 || offset<0 || (offset+nbytes) > UFS_BLOCK_SIZE*DIRECT_PTRS) return -1;
    void *mem = saddr + inodes[inum].direct[offset/UFS_BLOCK_SIZE]* UFS_BLOCK_SIZE + offset%UFS_BLOCK_SIZE;
    memcpy(mem, buffer, nbytes);
    inodes[inum].size += nbytes;
    return 0;
}

int server_read(int inum, char *buffer, int offset, int nbytes) {
    if (inum<0 || offset<0 || nbytes<0 || nbytes> BUFFER_SIZE || offset >= DIRECT_PTRS * UFS_BLOCK_SIZE) return -1;
    void* frm = saddr + (inodes[inum].direct[offset / UFS_BLOCK_SIZE]) * UFS_BLOCK_SIZE + offset%UFS_BLOCK_SIZE;
    memcpy(buffer, frm, nbytes);
    return 0;
}

int server_creat(int pinum, int type, char *name){
    if(pinum < 0) {
        return -1;
    }
    else if (strlen(name)>28) {
        return -1;
    }
    else if (type!=UFS_DIRECTORY && type != UFS_REGULAR_FILE) {
        return -1;
    }
    else if(server_lookup(pinum, name) != -1) { // name exists already
        return 0;
    }
    inode_t parent = inodes[pinum];
    if (parent.type != UFS_DIRECTORY) {
        return -1;
    }
    int ibit;
    int len = sup->num_inodes;
    for (int i = 0; i<len; i++){
        if(get_bit(ibaddr, i) == 0){
           ibit = i;
        }
    }
    int dbit;
    len = sup->num_data;
    for (int i = 0; i<len; i++){
        if(get_bit(dbaddr, i) == 0){
            dbit = i;
        }
    }

    MFS_DirEnt_t *dirs = (MFS_DirEnt_t *)(saddr + parent.direct[0] * UFS_BLOCK_SIZE);
    for (int j = 0; j<MAX_NUM_DIR; j++){
        if(dirs[j].inum == -1) {
            dirs[j].inum = ibit;
            strcpy(dirs[j].name, name);
            set_bit(ibaddr, ibit);
            break;
        }
    }

    parent.size+=sizeof(MFS_DirEnt_t);
    inodes[ibit].type = type; // INODE SET
    if(type == UFS_REGULAR_FILE) { // FILE
        inodes[ibit].size = 0;
        for (int i = 0; i<DIRECT_PTRS; i++){
            dbit = find_bit(dbaddr, sup->num_data);
            inodes[ibit].direct[i] = sup->data_region_addr + dbit;
            set_bit(dbaddr, dbit);
        }
    } else { // DIRECTORY
        inodes[ibit].size = 2 * sizeof(MFS_DirEnt_t);
        inodes[ibit].direct[0] = dbit + sup->data_region_addr;
        set_bit(dbaddr, dbit);
        MFS_DirEnt_t *dirs = (MFS_DirEnt_t *)(saddr + inodes[ibit].direct[0]*UFS_BLOCK_SIZE);
        dirs[0].inum = ibit;
        dirs[1].inum = pinum;
        strcpy(dirs[0].name, ".");
        strcpy(dirs[1].name, "..");
        for (int i = 2; i<MAX_NUM_DIR; i++){
            dirs[i].inum = -1;
        }
    }
    return 0;
}

int server_unlink(int pinum, char *name) {

    if (pinum < 0) {
        return -1;
    }
    else if (pinum > sup->num_inodes) {
        return -1;
    }
    inode_t *inode_p = &inodes[pinum];

    if (inode_p->type != UFS_DIRECTORY) {
        return -1;
    }

    int size_d = sizeof(MFS_DirEnt_t);
    int b = 0;
    int i,j;
    MFS_DirEnt_t *dir;
    for(i = 0; i < DIRECT_PTRS; i++) {
        if (inode_p->direct[i] != -1) {
            for (j = 0; j < UFS_BLOCK_SIZE; j = j + size_d) {
                int buf = (inode_p->direct[i])*UFS_BLOCK_SIZE + j;
                dir = (MFS_DirEnt_t *)((char *) saddr + buf);
                if (strcmp(dir->name, name) == 0) {
                    b=1;
                    break;
                }
            }
        }
        if (b){
            break;
        }
    }
    inode_t *inode = &inodes[dir->inum];
    if (inode->type == UFS_DIRECTORY) {
        for (int k = 2; k < UFS_BLOCK_SIZE / size_d; k++) {
            int buf = (inode->direct[i])*UFS_BLOCK_SIZE + j;
            MFS_DirEnt_t *inode_DirEnt = (MFS_DirEnt_t *)((char*)saddr + buf);
            if (inode_DirEnt->inum != -1){
                return -1;
            }
        }
    }
    dir->inum = -1;

    return 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);

    if (argc != 3) return -1;
    sd = UDP_Open(atoi(argv[1]));
    assert(sd > -1);
    fd = open(argv[2], O_RDWR);
    assert(fd > -1);
    struct stat sb;
    if(fstat(fd, &sb) < 0) return -1;

    saddr = mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    sup = (super_t *) saddr;
    ibaddr = saddr + sup->inode_bitmap_addr * UFS_BLOCK_SIZE;
    dbaddr = saddr + sup->data_bitmap_addr * UFS_BLOCK_SIZE;
    inodes = (inode_t *)(saddr + sup->inode_region_addr*UFS_BLOCK_SIZE);

    while (1) {
        struct sockaddr_in addr;
        message_t mess;
        int rc = UDP_Read(sd, &addr, (char*) &mess, sizeof(message_t));
        if (rc > 0)
        {
            switch(mess.mtype)
            {
                case MFS_LOOKUP:
                    mess.rc = server_lookup(mess.inum, mess.name);
                    UDP_Write(sd, &addr, (char*)&mess, sizeof(message_t));
                    break;
                case MFS_STAT:
                    mess.rc = server_stat(mess.inum);
                    mess.type = stype;
                    mess.nbytes = sbytes;
                    UDP_Write(sd, &addr, (char*) &mess, sizeof(message_t));
                    break;
                case MFS_WRITE:
                    mess.rc = server_write(mess.inum, mess.buffer, mess.offset, mess.nbytes);
                    UDP_Write(sd, &addr, (char*)&mess, sizeof(message_t));
                    break;
                case MFS_READ:
                    mess.rc = server_read(mess.inum,mess.buffer,mess.offset,mess.nbytes);
                    UDP_Write(sd, &addr, (char*) &mess, sizeof(message_t));
                    break;
                case MFS_CREAT:
                    mess.rc = server_creat(mess.inum,mess.type,mess.name);
                    UDP_Write(sd, &addr, (char*) &mess, sizeof(message_t));
                    break;
                case MFS_UNLINK:
                    mess.rc  = server_unlink(mess.inum,mess.name);
                    UDP_Write(sd, &addr, (char*) &mess, sizeof(message_t));
                    break;
                case MFS_SHUTDOWN:
                    msync(saddr, sb.st_size, MS_SYNC);
                    mess.rc = 0;
                    UDP_Write(sd, &addr, (char*) &mess, sizeof(message_t));
                    exit(0);
                    break;
            }
        }
        msync(saddr, sb.st_size, MS_SYNC);
    }
    return 0;
}

