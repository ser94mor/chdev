#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "chdev_common.h"

using namespace std;

void ioctl_test(int &fd) {
    unsigned int      buf_size = 0, /* size of chdev buffer */
                      num_item = 0; /* number of items in chdev buffer */
    struct chdev_item item;         /* used in read and write requests */
    char              buf[100];     /* buffer for read request */
    
    cout << "--ioctl(...)--" << endl;
    
    /* Write requests for /dev/chdev */
    string msg;
    cout << "Write requests:" << endl;
    for (int i = 1; i <= 5; i++) {
        msg = "Message #" + to_string(i);
        cout << msg << endl;
        item.buf  = const_cast<char *>(msg.c_str());
        item.size = msg.size() + 1; /* +1 because character with code 0 */
        if (ioctl(fd, CHDEV_IOCTL_SET_ITEM, &item)) {
            cerr << "ERROR: ioctl(fd, CHDEV_IOCTL_SET_ITEM, &item) returns non zero." << endl;
            exit(EXIT_FAILURE);
        }
    }
    
    /* Read requests for /dev/chdev */
    item.buf  = buf;
    item.size = 100;
    cout << "Read requests:" << endl;
    for (int i = 1; i <= 3; i++) {
        memset(item.buf, 0, 100 * sizeof(char));
        if (ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item)) {
            cerr << "ERROR: ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item) returns non zero." << endl;
            exit(EXIT_FAILURE);
        }
        cout << item.buf << endl;
    }
    
    /* Buffer size */
    if (ioctl(fd, CHDEV_IOCTL_GET_BUF_SIZE, &buf_size)) {
        cerr << "ERROR: ioctl(fd, CHDEV_IOCTL_GET_BUF_SIZE, &buf_size) returns non zero." << endl;
        exit(EXIT_FAILURE);    
    } 
    else {
        cout << "Buffer size:     " << buf_size << endl;
    }
    
    /* Number of items in the buffer */
    if (ioctl(fd, CHDEV_IOCTL_GET_NUM_ITEM, &num_item)) {
        cerr << "ERROR: ioctl(fd, CHDEV_IOCTL_GET_NUM_ITEM, &num_item) returns non zero." << endl;
        exit(EXIT_FAILURE);
    }
    else {
        cout << "Number of items: " << num_item << endl;
    }
    
    cout << endl;
}

int main() {
    
    int fd;                    /* file descriptor */
               
    cout << "CHDEV DRIVER TEST TOOLKIT" << endl << endl;
    
    /* /dev/chdev descriptor number */
    cout << "--File descriptor--" << endl;
    fd = open("/dev/chdev", O_RDWR);
    if (fd == -1) {
        cout << "ERROR: /dev/chdev file not found" << endl;
        exit(EXIT_FAILURE);
    }
    else {
        cout << "/dev/chdev: " << fd << endl;
    }
    cout << endl;
    
    /* Tests */
    ioctl_test(fd);
    
    cout << "ALL TESTS PASSED SUCCESSFULLY" << endl;
    
    return EXIT_SUCCESS;
}