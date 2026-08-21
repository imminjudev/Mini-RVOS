#ifndef FS_H
#define FS_H

#define FS_MAX_FILES       8
#define FS_MAX_OPEN_FILES 16
#define FS_NAME_MAX       32
#define FS_DATA_MAX       512

int fs_init(void);

int fs_create(
    const char *name,
    const char *data,
    unsigned long length
);

int fs_open(
    unsigned long pid,
    const char *name
);

long fs_read(
    unsigned long pid,
    int fd,
    void *buffer,
    unsigned long length
);

long fs_write(
    unsigned long pid,
    int fd,
    const void *buffer,
    unsigned long length
);

unsigned long fs_inode_count(void);

#endif
