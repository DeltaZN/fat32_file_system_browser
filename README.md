# FAT32 File System Browser
A program that can be used in two modes. The modes are set as command line arguments and allow:
1. Display a list of disks and partitions connected to the operating system.
2. Perform operations on the file system represented on a given disk, partition, or file.
The program running in the second mode performs the following actions:
1. Check whether the file system is supported on the specified partition or disk.
2. If the file system is supported, the program switches to the dialog
mode, waiting for the input of commands from the user. The commands specify operations on the file system:
a. output a list of names and attributes of elements of the specified directory;
b. copying files or directories from the file system directory;
c. displaying the name of the "current" directory and moving to another directory.
The program consist of two modules. The first module implements functions for working with the file system, and the second one implements user interaction.

## Example of a work session
```bash
user@host:~$ fsbrowse –l
/dev/sda
/dev/sda1
/dev/sda2
user@host:~$ fsbrowse –li
/dev/sda 100 GB
/dev/sda1 3.7 GB
/dev/sda2 96.3 GB ext3
user@host:~$ fsbrowse /dev/sda1
Unknown filesystem.
user@host:~$ fsbrowse /dev/sda2
# pwd
/
# ls
test1/
otherDirInFsRoot/
somefile
# cd /test1/test2
No such directory
# cd /test1
# ls
subdir1/
subdir2/
file1
file2
file3
# ls subdir2
testfile4
testfile5
testfile6
# pwd
/test1
# cp ./test2 /tmp/
Extracting /test1/test2/testfile4 to /tmp/test2/testfile4
Extracting /test1/test2/testfile5 to /tmp/test2/testfile5
Extracting /test1/test2/testfile6 to /tmp/test2/testfile6
# exit
user@host:~$ ls /tmp/test2
testfile4
testfile5
testfile6
user@host:~$
```