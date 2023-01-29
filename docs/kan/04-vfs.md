# Virtual File System
To allow for interfacing between real filesystems and the kernel, KanOS utilizes a virtual filesystem (VFS)
###  Functions
All functions are defined within `src/kan/vfs.c` and `include/kan/vfs.h`.
```c
int vfs_create(const char *pathname);
```
`vfs_create` is used to create files within a filesystem at a `<pathname>`.
If the file already exists, `vfs_create` returns `-1`, otherwise it returns a file descriptor to that<br>new file.
<hr>

```c
int vfs_remove(const char *pathname, const char *displace);
```
`vfs_remove` is used to remove files (which may also be directories) at a `<pathname>`<br>If `<pathname>` is a directory that contains files, and `<displace>` is also a directory, then<br>all the files within `<pathname>` are copied into `<displace>`.
<hr>

```c
int vfs_open(const char *pathname, short mode);
```
`vfs_open` is used to create a file descriptor that references the file at `<pathname>`. The created file descriptor will have the properties of `<mode>`. The various modes are specified in the [Modes](#modes) Section.
<hr>

```c
int vfs_close(int fd);
```
`vfs_close` closes a file descriptor. If the operation was successful, return `0`. Otherwise<br>return `-1`.
<hr>

```c
int vfs_copy(const char *destpath, const char *srcpath, short mode);
``` 
`vfs_copy` copies the contents of file `<srcpath>` to file `<destpath>`.
<hr>

```c
size_t vfs_read(int fd, void *buffer, size_t bytes);  
```
`vfs_read` reads `<bytes>` amount of bytes from file descriptor `<fd>` and saves them into <br>`<buffer>`. If the operation was successful, return the amount of bytes that were read. Otherwise, return `-1`.
<hr>

```c
size_t vfs_write(int fd, const void *buffer, size_t bytes);
```
`vfs_write` writes `<bytes>` amount of bytes to file descriptor `<fd>` from `<buffer>`.
If the operation was successful, return the amount of bytes that were read. Otherwise return `-1`.
<hr>

```c
uint64_t vfs_chown(const char *pathname, uint32_t uid, uin32_t gid); 
```
`vfs_chown` changes the owner or group of file `<pathname>` to `<uid>` and `<gid>` respectively. If the operation was successful, return `previous gid*32 |= previous uid`. Otherwise, return `-1`.
<hr>

```c
uint64_t vfs_chmod(const char *pathname, short permissions);
```
`vfs_chmod` changes the permissions of file `<pathname>` to `<permissions>`. The permissions needed may be specified by combining the predefined permissions. See [here](#permissions) for the list of predefined permissions. If the operation was successful, return the previous permissions. Otherwise, return `-1`. 

### Modes
The following is the list of file descriptor modes.
```c
#define O_CREATE 0x01
#define O_TRUNCATE 0x02
#define O_APPEND 0x04
#define O_READ 0x08
#define O_WRITE 0x10
#define O_RDWR 0x20
#define O_SYNC 0x40
```
`O_CREATE` creates a file if it does not already exist.
`O_TRUNCATE` truncates a preexisting file to length zero.
`O_APPEND` adds data to the end of a preexisting file with non-null contents.
`O_READ` reads from a file exclusively.
`O_WRITE` writes to a file exclusively.
`O_RDWR` both reads and writes to a file.
`O_SYNC` both reads and writes to a file, but waits for any IO operations that are effecting a file<br> to conclude before reading or writing.

### Permissions
The following is the list of file permissions.
```c
#define S_IRUSR 0x0400
#define S_IWUSR 0x0200
#define S_IXUSR 0x0100
#define S_IRGRP 0x0040
#define S_IWGRP 0x0020
#define S_IXGRP 0x0010
#define S_IROTH 0x0004
#define S_IWOTH 0x0002
#define S_IXOTH 0x0001
```
The `R`, `W` and `X` infixes signify Read, Write, and Execution permissions respectively.
The `USR`, `GRP` and `OTH` suffixes signify Owner, Group, and Others respectively.
