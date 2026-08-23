# Mini-RVOS Filesystem and User Shell

## 1. Overview

Mini-RVOS는 RAM에 존재하는 in-memory filesystem을 사용한다.

현재 filesystem은 다음 요소로 구성된다.

~~~text
inode table
    |
    +--> file name
    +--> file size
    +--> file data

open-file table
    |
    +--> PID
    +--> file descriptor
    +--> inode index
    +--> current offset
~~~

user shell은 U-mode에서 실행되며
filesystem과 UART 기능을 syscall을 통해 사용한다.

전체 흐름은 다음과 같다.

~~~text
U-mode shell
     |
     v
syscall
     |
     v
S-mode kernel
     |
     +--> UART
     |
     +--> filesystem
~~~

---

# 2. Filesystem Limits

현재 `fs.h`에는 다음 한계값이 정의되어 있다.

~~~c
#define FS_MAX_FILES       8
#define FS_MAX_OPEN_FILES 16
#define FS_NAME_MAX       32
#define FS_DATA_MAX       512
~~~

의미:

~~~text
FS_MAX_FILES
    최대 inode 수
    8

FS_MAX_OPEN_FILES
    전체 open-file slot 수
    16

FS_NAME_MAX
    file name buffer 크기
    32 bytes

FS_DATA_MAX
    file 하나의 최대 data 크기
    512 bytes
~~~

filesystem은 runtime에 동적으로 확장되지 않는다.

---

# 3. inode

현재 inode 구조:

~~~c
struct inode {
    int used;
    char name[FS_NAME_MAX];

    unsigned long size;
    unsigned char data[FS_DATA_MAX];
};
~~~

각 field:

~~~text
used
    inode slot 사용 여부

name
    file name

size
    현재 file 크기

data
    file contents
~~~

이다.

---

# 4. inode Table

filesystem은 다음 static array를 사용한다.

~~~c
static struct inode
    inodes[FS_MAX_FILES];
~~~

구조:

~~~text
inodes

[0] file
[1] file
[2] unused
[3] unused
...
[7] unused
~~~

새 파일을 만들 때 `used == 0`인 slot을 찾는다.

---

# 5. File Data Storage

file contents도 inode 내부 배열에 저장된다.

~~~c
unsigned char data[FS_DATA_MAX];
~~~

따라서 현재 파일 하나가 저장할 수 있는 data는 최대:

~~~text
512 bytes
~~~

다.

file data를 위한 block allocator나 disk block은 없다.

---

# 6. inode_count

현재 filesystem은:

~~~c
static unsigned long inode_count;
~~~

를 유지한다.

새 inode가 생성되면:

~~~text
inode_count++
~~~

된다.

조회 함수:

~~~c
fs_inode_count()
~~~

가 존재한다.

---

# 7. File Lookup

file name으로 inode를 찾는 함수:

~~~c
find_inode()
~~~

는 inode table을 처음부터 끝까지 순회한다.

개념:

~~~text
requested name
      |
      v
inode[0]
      |
      +--> match?
      |
      v
inode[1]
      |
      +--> match?
      |
      v
...
~~~

현재 최대 inode 수가 8개이므로
linear scan을 사용한다.

---

# 8. File Names

현재 filename은 inode 내부:

~~~c
char name[FS_NAME_MAX];
~~~

에 저장된다.

`copy_name()`은 최대:

~~~text
FS_NAME_MAX - 1
~~~

개의 문자를 복사하고
마지막에 NUL character를 넣는다.

허용 범위를 넘는 이름은 생성에 실패한다.

---

# 9. fs_create()

file 생성 함수:

~~~c
int fs_create(
    const char *name,
    const char *data,
    unsigned long length)
~~~

주요 검사:

~~~text
name != NULL

length <= FS_DATA_MAX

length > 0이면 data != NULL

같은 이름의 file이 존재하지 않음

빈 inode slot이 존재함
~~~

조건을 만족하면 inode를 초기화한다.

---

# 10. File Creation Flow

`fs_create()` 흐름:

~~~text
validate arguments
      |
      v
check duplicate name
      |
      v
find unused inode
      |
      v
copy filename
      |
      v
copy data
      |
      v
set size
      |
      v
used = 1
      |
      v
inode_count++
~~~

성공하면 inode table index를 반환한다.

실패하면:

~~~text
-1
~~~

을 반환한다.

---

# 11. fs_init()

boot 중 `kernel_main()`은:

~~~text
fs_init()
~~~

을 호출한다.

`fs_init()`은:

~~~text
inode_count = 0
      |
      v
mark all inode slots unused
      |
      v
mark all open-file slots unused
      |
      v
create /hello.txt
~~~

순서로 진행된다.

---

# 12. Default File

filesystem initialization 시 다음 file이 만들어진다.

~~~text
/hello.txt
~~~

내용:

~~~text
Hello from /hello.txt in Mini-RVOS!
~~~

따라서 shell에서:

~~~text
cat /hello.txt
~~~

을 실행하면 해당 내용을 읽을 수 있다.

---

# 13. Volatile Storage

현재 filesystem data는 kernel RAM 내부에 존재한다.

QEMU가 종료되면 모든 file state가 사라진다.

예:

~~~text
boot
 |
 v
filesystem initialized
 |
 v
files exist in RAM
 |
 v
QEMU shutdown
 |
 v
data lost
~~~

다음 boot에서는 `fs_init()`이 다시 초기 상태를 만든다.

---

# 14. Open File

inode와 open file은 서로 다른 정보를 가진다.

현재 open-file 구조:

~~~c
struct open_file {
    int used;

    unsigned long pid;
    int fd;
    int inode_index;

    unsigned long offset;
};
~~~

각 field:

~~~text
used
    slot 사용 여부

pid
    이 open instance의 process

fd
    process가 사용하는 file descriptor

inode_index
    연결된 inode

offset
    현재 read/write position
~~~

이다.

---

# 15. inode and Open File

하나의 inode는 file 자체의 정보를 가진다.

~~~text
inode

name
size
data
~~~

open-file entry는 특정 process가
그 file을 연 상태를 가진다.

~~~text
open_file

pid
fd
inode index
offset
~~~

관계:

~~~text
Process
   |
   | fd
   v
Open File
   |
   | inode_index
   v
Inode
   |
   +--> name
   +--> size
   +--> data
~~~

---

# 16. File Descriptor

user program은 inode index를 직접 사용하지 않는다.

`fs_open()`의 반환값인 file descriptor를 사용한다.

예:

~~~text
fd = open("/hello.txt")
~~~

이후:

~~~text
read(fd, ...)
close(fd)
~~~

형태로 접근한다.

---

# 17. FD Allocation

현재 filesystem file descriptor는:

~~~text
3
~~~

부터 시작한다.

`fs_open()`은 현재 PID에서 사용 중이지 않은
가장 작은 fd를 찾는다.

예:

~~~text
first open
    fd = 3

second open
    fd = 4

close fd 3

next open
    fd = 3
~~~

형태가 가능하다.

---

# 18. stdin and stdout

Mini-RVOS syscall layer에서는:

~~~text
fd 0
    stdin

fd 1
    stdout
~~~

을 사용한다.

`fd 0` read는 UART input으로 연결된다.

`fd 1` write는 UART output으로 연결된다.

filesystem에서 `fs_open()`으로 반환하는 fd는
3부터 시작한다.

구조:

~~~text
fd 0
    UART stdin

fd 1
    UART stdout

fd 3+
    filesystem open files
~~~

---

# 19. PID and File Descriptors

open-file entry에는:

~~~c
unsigned long pid;
~~~

가 저장된다.

같은 fd 번호라도 PID가 다르면
서로 다른 open-file entry를 나타낼 수 있다.

예:

~~~text
PID 1
    fd 3 -> inode A

PID 2
    fd 3 -> inode B
~~~

lookup은:

~~~text
PID + FD
~~~

조합으로 이루어진다.

---

# 20. Global Open-File Table

현재 open-file table:

~~~c
static struct open_file
    open_files[FS_MAX_OPEN_FILES];
~~~

이다.

전체 system에서 사용할 수 있는 open-file slot은:

~~~text
16
~~~

개다.

각 slot에 PID가 저장되므로
여러 process의 open 상태가 같은 table에 함께 존재할 수 있다.

---

# 21. fs_open()

`fs_open()` 흐름:

~~~text
find inode by name
      |
      v
find free FD for PID
      |
      v
find unused open-file slot
      |
      v
store PID
      |
      v
store FD
      |
      v
store inode index
      |
      v
offset = 0
      |
      v
return FD
~~~

file을 새로 열면 offset은 항상 0에서 시작한다.

---

# 22. File Offset

offset은 다음 read 또는 write가 시작될 위치다.

예:

~~~text
file data

ABCDE
^
offset = 0
~~~

2 bytes를 읽으면:

~~~text
read -> AB

ABCDE
  ^
offset = 2
~~~

다음 read는 `C`부터 시작한다.

---

# 23. Why Offset Belongs to Open File

같은 inode를 여러 번 열 수 있다.

예:

~~~text
Open instance A
    inode = /hello.txt
    offset = 5

Open instance B
    inode = /hello.txt
    offset = 0
~~~

file data는 같은 inode에 존재하고,
read/write position은 각 open instance가 가진다.

따라서 offset은 inode가 아니라
open-file entry에 저장된다.

---

# 24. find_open_file()

open file을 찾을 때:

~~~text
PID
+
FD
~~~

를 함께 검사한다.

조건:

~~~c
open_files[i].used &&
open_files[i].pid == pid &&
open_files[i].fd == fd
~~~

가 일치하는 entry를 반환한다.

---

# 25. fs_read()

현재 interface:

~~~c
long fs_read(
    unsigned long pid,
    int fd,
    void *buffer,
    unsigned long length)
~~~

이다.

먼저 PID와 FD를 이용해
open-file entry를 찾는다.

---

# 26. Read Flow

전체 read 흐름:

~~~text
PID + FD
    |
    v
open-file entry
    |
    v
inode index
    |
    v
inode
    |
    +--> current offset
    |
    +--> remaining bytes
    |
    v
copy data to buffer
    |
    v
advance offset
~~~

---

# 27. End of File

현재 offset이 file size 이상이면:

~~~c
return 0;
~~~

한다.

즉:

~~~text
read return > 0
    bytes read

read return 0
    end of file

read return -1
    error
~~~

형태다.

---

# 28. Read Length

requested length가 file에 남은 data보다 크면
남은 data만 읽는다.

계산:

~~~text
remaining
    = inode size - current offset

count
    = min(requested length, remaining)
~~~

이다.

---

# 29. Updating Read Offset

data를 복사한 후:

~~~c
file->offset += count;
~~~

를 수행한다.

따라서 다음 `fs_read()`는
이전 read 이후 위치에서 시작한다.

---

# 30. fs_write()

현재 interface:

~~~c
long fs_write(
    unsigned long pid,
    int fd,
    const void *buffer,
    unsigned long length)
~~~

이다.

write 역시 PID와 FD를 이용해
open-file entry를 찾는다.

---

# 31. Write Flow

~~~text
PID + FD
    |
    v
open-file entry
    |
    v
inode
    |
    v
calculate available space
    |
    v
copy buffer into inode data
    |
    v
advance offset
    |
    v
update inode size if needed
~~~

---

# 32. File Size Growth

write 후 offset이 기존 file size보다 커지면:

~~~c
inode->size =
    file->offset;
~~~

으로 file 크기를 늘린다.

예:

~~~text
old size
    10

offset
    10

write
    5 bytes

new offset
    15

new size
    15
~~~

---

# 33. Maximum File Size

inode data array는:

~~~text
FS_DATA_MAX = 512
~~~

bytes다.

write가 file capacity를 넘어가면
남은 공간만 기록한다.

offset이 이미 512 이상이면:

~~~text
0
~~~

을 반환한다.

---

# 34. fs_close()

close는 PID와 FD로
open-file entry를 찾는다.

찾은 entry의 값을 초기화한다.

~~~text
used = 0
pid = 0
fd = 0
inode_index = 0
offset = 0
~~~

inode 자체는 삭제하지 않는다.

따라서 file data는 계속 존재한다.

---

# 35. Close and Inode Lifetime

예:

~~~text
open /hello.txt
       |
       v
open-file entry created
       |
       v
close
       |
       v
open-file entry released
~~~

이 과정에서:

~~~text
/hello.txt inode
~~~

는 그대로 유지된다.

다시 open하면 새로운 open-file entry가 생성된다.

---

# 36. Filesystem Syscalls

user code가 filesystem 함수들을 직접 호출하지 않는다.

syscall layer를 거친다.

현재 관련 syscall:

~~~text
SYS_OPEN
SYS_READ
SYS_CREATE
SYS_WRITE
SYS_CLOSE
~~~

흐름:

~~~text
U-mode
 |
 v
ecall
 |
 v
syscall_handle()
 |
 v
filesystem function
~~~

---

# 37. Path Transfer

`open()`과 `create()`는
user virtual address에 있는 path string을 전달받는다.

kernel은 user path를 kernel-local buffer로 복사한다.

구조:

~~~text
user path
   |
   v
validate user address
   |
   v
copy byte by byte
   |
   v
kernel path buffer
   |
   v
fs_open() / fs_create()
~~~

filesystem implementation은
kernel buffer에 있는 string을 사용한다.

---

# 38. User Buffer and File Read

file read 경로:

~~~text
inode data
    |
    v
fs_read()
    |
    v
user buffer
~~~

syscall layer는 먼저 user buffer가
writable user memory인지 검사한다.

검사가 끝난 뒤 filesystem이
해당 buffer로 data를 복사한다.

---

# 39. User Buffer and File Write

file write 경로:

~~~text
user buffer
    |
    v
fs_write()
    |
    v
inode data
~~~

syscall layer는 user buffer가
readable user memory인지 검사한다.

이후 filesystem이 buffer 내용을
inode data array로 복사한다.

---

# 40. User Shell

Mini-RVOS shell은 U-mode에서 실행된다.

entry:

~~~text
user_entry
    |
    v
user_shell_main()
~~~

`user_entry.S`는:

~~~asm
user_entry:
    call user_shell_main
~~~

형태로 shell main function을 호출한다.

---

# 41. User Code Sections

shell code에는:

~~~c
USER_TEXT
~~~

attribute가 사용된다.

string과 read-only data에는:

~~~c
USER_RODATA
~~~

가 사용된다.

linker는 이를 각각 user memory region으로 배치한다.

process 생성 시 해당 region은
private physical pages로 복사된다.

---

# 42. Shell Initialization

`user_shell_main()`이 시작되면:

~~~text
print banner
      |
      v
enter command loop
~~~

banner:

~~~text
Mini-RVOS shell
Type 'help' for commands.
~~~

이 출력된다.

---

# 43. Shell Loop

현재 shell loop:

~~~text
print "$ "
     |
     v
read stdin
     |
     v
NUL-terminate command
     |
     v
parse command
     |
     v
execute command
     |
     v
repeat
~~~

이다.

---

# 44. Command Buffer

현재 command buffer:

~~~c
char command[128];
~~~

이다.

stdin read에서는:

~~~text
127 bytes
~~~

까지 요청한다.

마지막 한 byte는 C string의 NUL character를 위해 남긴다.

read 후:

~~~c
command[length] = '\0';
~~~

을 수행한다.

---

# 45. stdin

shell input:

~~~c
user_read(
    0,
    command,
    sizeof(command) - 1
);
~~~

에서:

~~~text
fd = 0
~~~

을 사용한다.

kernel의 `SYS_READ`가 fd 0을 확인하고
UART input path로 연결한다.

---

# 46. UART Line Input

현재 stdin implementation은 다음 동작을 지원한다.

~~~text
printable ASCII

Enter

Backspace

Delete

Ctrl-D
~~~

입력 문자는 UART에서 읽고
화면에도 echo한다.

---

# 47. stdout

shell 출력은:

~~~c
user_write(
    1,
    buffer,
    length
);
~~~

형태다.

~~~text
fd = 1
~~~

이므로 kernel syscall layer가 UART로 출력한다.

---

# 48. write_text()

shell 내부에는:

~~~c
write_text()
~~~

helper가 있다.

동작:

~~~text
calculate string length
      |
      v
SYS_WRITE
      |
      v
fd 1
      |
      v
UART
~~~

이다.

---

# 49. Current Shell Commands

현재 shell command:

~~~text
help
pid
echo
cat
memtest
exit
~~~

이다.

`help` 출력:

~~~text
commands: help pid echo cat exit memtest
~~~

이다.

---

# 50. help

입력:

~~~text
help
~~~

이면 help string을 stdout으로 출력한다.

실행 흐름:

~~~text
command parser
      |
      v
string_equal("help")
      |
      v
write_text(help_text)
~~~

---

# 51. pid

입력:

~~~text
pid
~~~

이면:

~~~text
SYS_GETPID
~~~

를 호출한다.

현재 interactive shell process는:

~~~text
PID 1
~~~

이다.

따라서 현재 실행 결과:

~~~text
1
~~~

이 출력된다.

---

# 52. PID Conversion

`SYS_GETPID`는 integer 값을 반환한다.

shell은 이를 decimal ASCII로 변환한다.

예:

~~~text
123
~~~

값에 대해 먼저 digit을 역순으로 저장한다.

~~~text
3
2
1
~~~

그 후 다시 뒤집어서:

~~~text
1
2
3
~~~

순서로 출력 buffer에 넣는다.

---

# 53. echo

입력:

~~~text
echo
~~~

이면 newline을 출력한다.

입력:

~~~text
echo hello
~~~

이면:

~~~text
hello
~~~

를 출력한다.

parser는:

~~~text
"echo "
~~~

prefix를 검사한 후
그 뒤의 문자열을 stdout으로 보낸다.

---

# 54. cat

입력:

~~~text
cat /hello.txt
~~~

의 흐름:

~~~text
shell
  |
  v
SYS_OPEN
  |
  v
fs_open()
  |
  v
FD
  |
  v
SYS_READ
  |
  v
fs_read()
  |
  v
user buffer
  |
  v
SYS_WRITE fd 1
  |
  v
UART
  |
  v
SYS_CLOSE
~~~

이다.

---

# 55. cat Buffer

`command_cat()`은:

~~~c
char buffer[128];
~~~

을 사용한다.

file을 반복해서 최대 128 bytes씩 읽는다.

~~~text
read
 |
 +--> count > 0
 |       |
 |       v
 |     stdout
 |       |
 |       v
 |     read again
 |
 +--> count == 0
         |
         v
        EOF
~~~

---

# 56. cat Error Handling

path가 없으면:

~~~text
usage: cat <path>
~~~

를 출력한다.

open이 실패하면:

~~~text
cat: file not found
~~~

를 출력한다.

read가 실패해도 현재 shell에서는
같은 error message를 사용한다.

---

# 57. memtest

`memtest`는 syscall user-pointer validation을
확인하기 위한 shell command다.

현재 세 가지 case를 검사한다.

~~~text
invalid address

kernel address

read-only user page used as read destination
~~~

---

# 58. memtest Case 1

첫 번째 test:

~~~text
SYS_WRITE

buffer address = 1
length = 1
~~~

address 1은 valid user mapping이 아니다.

kernel은:

~~~text
-1
~~~

을 반환해야 한다.

---

# 59. memtest Case 2

두 번째 test:

~~~text
buffer =
    0x80200000
~~~

이 주소는 kernel region이다.

process page table에 kernel mapping이 존재하지만:

~~~text
PTE_U = 0
~~~

이므로 user buffer로 사용할 수 없다.

kernel은:

~~~text
-1
~~~

을 반환해야 한다.

---

# 60. memtest Case 3

세 번째 test는 stdin read destination으로:

~~~text
banner
~~~

주소를 전달한다.

banner는 user rodata에 있으므로:

~~~text
PTE_W = 0
~~~

이다.

`read()`는 destination buffer에 data를 써야 하므로
writable page가 필요하다.

kernel은 UART input을 기다리기 전에:

~~~text
-1
~~~

을 반환해야 한다.

---

# 61. memtest Result

세 test가 모두 예상대로 실패하면:

~~~text
[OK] user pointer validation
~~~

을 출력한다.

하나라도 예상과 다르면:

~~~text
[FAIL] user pointer validation
~~~

을 출력한다.

---

# 62. exit

입력:

~~~text
exit
~~~

이면:

~~~text
SYS_EXIT
~~~

을 호출한다.

현재 kernel 동작:

~~~text
print exit message
      |
      v
disable timer interrupt
      |
      v
wfi loop
~~~

이다.

현재 process memory와 filesystem resource를
회수하는 process teardown은 구현되어 있지 않다.

---

# 63. Unknown Command

등록된 command와 일치하지 않으면:

~~~text
unknown command
~~~

을 출력하고 다음 prompt로 돌아간다.

---

# 64. Shell Parsing

현재 parser는 별도의 token structure를 만들지 않는다.

사용하는 방식:

~~~text
exact string comparison

prefix comparison
~~~

이다.

예:

~~~text
"pid"
    exact comparison

"echo "
    prefix comparison

"cat "
    prefix comparison
~~~

현재 command set에 맞춘 parser 구조다.

---

# 65. User-Space String Functions

shell은 일반 C standard library에 의존하지 않는다.

현재 직접 구현된 helper:

~~~text
string_length()

string_equal()

string_starts_with()
~~~

를 사용한다.

Mini-RVOS user environment에는 libc가 없다.

---

# 66. Shell and Privilege Separation

shell은 U-mode에서 동작한다.

따라서 다음 kernel object에 직접 접근하지 않는다.

~~~text
inode table

open-file table

current_process

UART device register
~~~

shell은 syscall ABI를 사용한다.

~~~text
U-mode shell
     |
     v
ecall
     |
     v
S-mode syscall handler
     |
     v
kernel subsystem
~~~

---

# 67. Complete cat Path

`cat /hello.txt` 한 번의 전체 흐름:

~~~text
U-mode shell
      |
      v
parse "cat /hello.txt"
      |
      v
SYS_OPEN
      |
      v
trap
      |
      v
copy user path
      |
      v
fs_open(PID 1, "/hello.txt")
      |
      v
fd 3
      |
      v
return to U-mode
      |
      v
SYS_READ(fd 3)
      |
      v
trap
      |
      v
validate writable buffer
      |
      v
fs_read()
      |
      v
inode data -> user buffer
      |
      v
return byte count
      |
      v
SYS_WRITE(fd 1)
      |
      v
trap
      |
      v
validate readable buffer
      |
      v
UART output
      |
      v
SYS_READ again
      |
      v
EOF returns 0
      |
      v
SYS_CLOSE(fd 3)
~~~

이 흐름은 process, VM, trap, syscall,
filesystem, UART가 함께 동작하는 경로다.

---

# 68. Filesystem State Model

현재 filesystem state를 정리하면:

~~~text
Filesystem

+---------------------------+
| inode table               |
|                           |
| [0] /hello.txt            |
| [1] ...                   |
| [7] ...                   |
+---------------------------+

+---------------------------+
| open-file table           |
|                           |
| PID | FD | inode | offset |
| ...                       |
+---------------------------+
~~~

inode table은 file contents를 저장한다.

open-file table은 process별 active access state를 저장한다.

---

# 69. Current Filesystem Limitations

현재 filesystem에는 다음 기능이 없다.

## Directory Hierarchy

`/hello.txt`라는 name을 사용할 수 있지만
directory tree 구조를 구현하지 않는다.

---

## Persistent Storage

VirtIO block device나 disk filesystem이 없다.

QEMU 종료 후 data는 유지되지 않는다.

---

## Dynamic File Size

각 file은 최대 512 bytes다.

---

## Dynamic Inode Allocation

inode table은 8개 slot로 고정되어 있다.

---

## File Deletion

현재:

~~~text
unlink
remove
~~~

기능이 없다.

---

## Seek

현재:

~~~text
lseek
~~~

가 없다.

offset은 read/write에 따라 앞으로 이동한다.

---

## File Permissions

filesystem 자체의:

~~~text
owner
group
mode bits
~~~

는 없다.

---

## Directory Operations

현재 다음 기능이 없다.

~~~text
mkdir
rmdir
readdir
chdir
getcwd
~~~

---

## Concurrent Synchronization

filesystem table을 보호하는 lock이 없다.

현재 single-hart configuration에 맞춰져 있다.

---

# 70. Current Shell Limitations

현재 shell에는 다음 기능이 없다.

~~~text
pipes

redirection

background jobs

environment variables

working directory

command history

external executable loading

multiple user programs
~~~

현재 command는 shell binary 내부에 구현되어 있다.

---

# 71. Why the Current Filesystem Is Useful

현재 filesystem 구조를 통해 다음 관계를 확인할 수 있다.

~~~text
file
    inode

opened file
    open-file entry

process-visible handle
    file descriptor

sequential position
    offset
~~~

또한 syscall을 통해 U-mode process가
kernel filesystem을 사용하는 경로를 확인할 수 있다.

---

# 72. Filesystem Mental Model

현재 Mini-RVOS filesystem:

~~~text
inode
    file identity + contents

open_file
    process-specific open state

fd
    user-visible handle

offset
    current read/write position
~~~

관계:

~~~text
PID + FD
     |
     v
open_file
     |
     v
inode
     |
     v
data
~~~

이다.

---

# 73. Shell Mental Model

현재 Mini-RVOS shell:

~~~text
UART input
     |
     v
SYS_READ fd 0
     |
     v
U-mode command buffer
     |
     v
command parser
     |
     +--> help
     +--> pid
     +--> echo
     +--> cat
     +--> memtest
     +--> exit
     |
     v
syscalls
     |
     v
kernel
~~~

이다.

---

# 74. Connection to the Whole OS

Mini-RVOS shell까지 도달하는 전체 구조:

~~~text
OpenSBI
   |
   v
Boot
   |
   v
Physical Memory Manager
   |
   v
Sv39 Virtual Memory
   |
   v
Process
   |
   v
U-mode
   |
   v
User Shell
   |
   v
ecall
   |
   v
Trap
   |
   v
Syscall
   |
   +--> UART
   |
   +--> Filesystem
~~~

shell은 앞에서 구현한 subsystem들이
하나의 실행 경로로 연결되는 user-space interface다.
