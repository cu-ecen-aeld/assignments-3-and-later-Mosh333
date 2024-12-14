# Assignment 7 Step 3f and 3i Output and analysis

### Analysis for the oops error from `echo "hello world" > /dev/null`:
We can see from the oops message that there was a `NULL pointer dereference` error causing the kernel to crash.
The crash originated from `pc : faulty_write+0x14/0x20 [faulty]` (offset of `0x14`). So there is a function called
faulty_write that crashed 20 bytes (`0x14`) into the `faulty_write` function with function total size being 32 bytes (`0x20`).
We should expect the error for the null pointer dereference error to be somewhere in the middle of the function.

Checking in https://github.com/cu-ecen-aeld/ldd3/blob/master/misc-modules/faulty.c:
```
ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```
it is right in the middle of the function (`*(int *)0 = 0;`).
Further detailed analysis can be done by dissassembling the file: `./buildroot/output/target/lib/modules/5.15.18/extra/faulty.ko`
using objdump.

```
$ ./buildroot/output/host/bin/aarch64-buildroot-linux-uclibc-objdump -d ./buildroot/output/target/lib/modules/5.15.18/extra/faulty.ko > faulty_disassembly.txt
$ cat faulty_disassembly.txt
...
0000000000000000 <faulty_write>:
   0:	d503245f 	bti	c
   4:	d2800001 	mov	x1, #0x0                   	// #0
   8:	d2800000 	mov	x0, #0x0                   	// #0
   c:	d503233f 	paciasp
  10:	d50323bf 	autiasp
  14:	b900003f 	str	wzr, [x1]
  18:	d65f03c0 	ret
  1c:	d503201f 	nop
...
```
We can see that this breaks because the instruction at `0x14` (`str wzr, [x1]`) attempts to store the value of the zero register (`wzr`) at the memory address stored in `x1`. Since `x1` is explicitly set to `0x0` (`mov x1, #0x0`), this results in a NULL pointer dereference, causing the kernel to crash.

### Output from `echo "hello world" > /dev/null`:
```
Welcome to Buildroot
buildroot login: root
Password: 
# lsmod
Module                  Size  Used by    Tainted: G  
hello                  16384  0 
faulty                 16384  0 
scull                  24576  0 
# echo "hello_world" > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=00000000420c1000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 165 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d23d80
x29: ffffffc008d23d80 x28: ffffff80020dcc80 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 000000000000000c x21: 0000005587882670
x20: 0000005587882670 x19: ffffff8002085b00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 16c3c774761399e1 ]---

Welcome to Buildroot
buildroot login: 

```