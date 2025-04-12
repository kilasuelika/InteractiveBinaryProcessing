# `ibp` —— Interactive binary file processing tool

## Motivation

When doing research, I frequently needed to dump some intermediate data for later analysis. When data size is large, dump text file will be very slow and occupy too much disk space, so I usually dump binary data then analysis with Python. But I find reading binary file with python can be cubersome and boring so I decide to write my own binary file analysis tool. The tool is motivated by text processing tool `awk` and debug tool `windbg`. It also has some similar functionalities with [fq](https://github.com/wader/fq) and [GNU poke](https://jemarch.net/poke) but `ibp` fullfil my personal usage better.

## Feature

1. Easy commands to read and show binary file data.
1. Interated with `Exprtk` for expression evaluation
1. Script programming support.
1. 


## usage
### Install or Build

Just download the Release folder, click the `ibp.exe` then a prompt will shows. Type following commands for running.


### Basic file reading

```bash
# load a file
ld xx.bin

# read int
r i32
# read 4 int
r i32*4
# read int then int64
r i32 i64
# write to variable a
r i32(a)
# do some calculation
a+3
# write to array a
r i32*4(a)
# allow in-place calculator
r i32*${3+2}(a)

# read and move pointer
rv i32
```


### Shell Commands

Run shell commands:
```
x=3
sh echo ${x}
```
Note that `${x}` will be first evaluated by `ibp` and then send the result commands to shell. Thus the command executed by shell is `echo 3`.

