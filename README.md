chdev
=====
**Linux character device driver**. The driver allows you to work with the fixed size circular buffer. **chdev** stores items which, for example, could be strings or any other sort of information presented as a sequence of bytes. Despite the fact that the driver was written for educational purposes, many useful aspects of Linux device drivers programming were touched:

* dynamic major
* ioctl
* */proc* file system
* GNUmakefile + Kbuild system

There are also small test suit is provided. It shows how to invoke the character driver which has been already loaded to the system. А detailed description of chdev driver can be obtained by contacting me.

--------------------------
**Author:** Sergey Morozov
