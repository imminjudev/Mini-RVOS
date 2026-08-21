#include "../include/fs.h"

struct inode {
    int used;
    char name[FS_NAME_MAX];

    unsigned long size;
    unsigned char data[FS_DATA_MAX];
};

struct open_file {
    int used;

    unsigned long pid;
    int fd;
    int inode_index;

    unsigned long offset;
};

static struct inode inodes[FS_MAX_FILES];
static struct open_file open_files[FS_MAX_OPEN_FILES];

static unsigned long inode_count;

static int string_equal(
    const char *a,
    const char *b)
{
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static int find_inode(
    const char *name)
{
    for (unsigned long i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (!inodes[i].used) {
            continue;
        }

        if (string_equal(
                inodes[i].name,
                name)) {
            return (int)i;
        }
    }

    return -1;
}

static int copy_name(
    char *destination,
    const char *source)
{
    unsigned long i;

    for (i = 0;
         i < FS_NAME_MAX - 1 &&
         source[i] != '\0';
         i++) {

        destination[i] = source[i];
    }

    destination[i] = '\0';

    return source[i] == '\0'
        ? 0
        : -1;
}

int fs_create(
    const char *name,
    const char *data,
    unsigned long length)
{
    if (name == 0 ||
        length > FS_DATA_MAX) {
        return -1;
    }

    if (length > 0 &&
        data == 0) {
        return -1;
    }

    if (find_inode(name) >= 0) {
        return -1;
    }

    for (unsigned long i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (inodes[i].used) {
            continue;
        }

        if (copy_name(
                inodes[i].name,
                name) != 0) {
            return -1;
        }

        for (unsigned long j = 0;
             j < length;
             j++) {

            inodes[i].data[j] =
                (unsigned char)data[j];
        }

        inodes[i].size = length;
        inodes[i].used = 1;

        inode_count++;

        return (int)i;
    }

    return -1;
}

int fs_init(void)
{
    inode_count = 0;

    for (unsigned long i = 0;
         i < FS_MAX_FILES;
         i++) {

        inodes[i].used = 0;
        inodes[i].size = 0;
    }

    for (unsigned long i = 0;
         i < FS_MAX_OPEN_FILES;
         i++) {

        open_files[i].used = 0;
    }

    static const char hello[] =
        "Hello from /hello.txt in Mini-RVOS!\n";

    if (fs_create(
            "/hello.txt",
            hello,
            sizeof(hello) - 1) < 0) {

        return -1;
    }

    return 0;
}

static int fd_in_use(
    unsigned long pid,
    int fd)
{
    for (unsigned long i = 0;
         i < FS_MAX_OPEN_FILES;
         i++) {

        if (open_files[i].used &&
            open_files[i].pid == pid &&
            open_files[i].fd == fd) {

            return 1;
        }
    }

    return 0;
}

int fs_open(
    unsigned long pid,
    const char *name)
{
    int inode_index =
        find_inode(name);

    if (inode_index < 0) {
        return -1;
    }

    int fd = 3;

    while (fd < 3 + FS_MAX_OPEN_FILES &&
           fd_in_use(pid, fd)) {

        fd++;
    }

    if (fd >=
        3 + FS_MAX_OPEN_FILES) {

        return -1;
    }

    for (unsigned long i = 0;
         i < FS_MAX_OPEN_FILES;
         i++) {

        if (open_files[i].used) {
            continue;
        }

        open_files[i].used = 1;
        open_files[i].pid = pid;
        open_files[i].fd = fd;
        open_files[i].inode_index =
            inode_index;
        open_files[i].offset = 0;

        return fd;
    }

    return -1;
}

static struct open_file *find_open_file(
    unsigned long pid,
    int fd)
{
    for (unsigned long i = 0;
         i < FS_MAX_OPEN_FILES;
         i++) {

        if (open_files[i].used &&
            open_files[i].pid == pid &&
            open_files[i].fd == fd) {

            return &open_files[i];
        }
    }

    return 0;
}

long fs_read(
    unsigned long pid,
    int fd,
    void *buffer,
    unsigned long length)
{
    if (buffer == 0) {
        return -1;
    }

    struct open_file *file =
        find_open_file(pid, fd);

    if (file == 0) {
        return -1;
    }

    struct inode *inode =
        &inodes[file->inode_index];

    if (file->offset >=
        inode->size) {

        return 0;
    }

    unsigned long remaining =
        inode->size -
        file->offset;

    unsigned long count =
        length < remaining
        ? length
        : remaining;

    unsigned char *destination =
        (unsigned char *)buffer;

    for (unsigned long i = 0;
         i < count;
         i++) {

        destination[i] =
            inode->data[
                file->offset + i
            ];
    }

    file->offset += count;

    return (long)count;
}

long fs_write(
    unsigned long pid,
    int fd,
    const void *buffer,
    unsigned long length)
{
    if (buffer == 0) {
        return -1;
    }

    struct open_file *file =
        find_open_file(pid, fd);

    if (file == 0) {
        return -1;
    }

    struct inode *inode =
        &inodes[file->inode_index];

    if (file->offset >=
        FS_DATA_MAX) {

        return 0;
    }

    unsigned long available =
        FS_DATA_MAX -
        file->offset;

    unsigned long count =
        length < available
        ? length
        : available;

    const unsigned char *source =
        (const unsigned char *)buffer;

    for (unsigned long i = 0;
         i < count;
         i++) {

        inode->data[
            file->offset + i
        ] = source[i];
    }

    file->offset += count;

    if (file->offset >
        inode->size) {

        inode->size =
            file->offset;
    }

    return (long)count;
}

unsigned long fs_inode_count(void)
{
    return inode_count;
}
