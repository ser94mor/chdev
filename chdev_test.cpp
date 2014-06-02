/* 
 * Copyright (C) 2014 Sergey Morozov
 * 
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.
 * 
 * Compile this source with:
 *     g++ -std=c++11 chdev_test.cpp -o test_chdev
 */

#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "chdev_common.h"

#define ITEM_SIZE 100

using namespace std;

/*
 * Item variable for read and write requests
 */
char buf[ITEM_SIZE];
struct chdev_item item {
    .buf = buf,
    .size = ITEM_SIZE
};

int write_test(string &msg, int &fd) {
    int status = 0;  /* status of write request */
    cout << "WRITE   { ";
    
    item.buf  = const_cast<char *>(msg.c_str());
    item.size = msg.size() + 1; /* +1 because character with code 0 */
    
    status = ioctl(fd, CHDEV_IOCTL_SET_ITEM, &item); /* send write request */
    switch (status) {
        case -ENOMEM:
            cerr << "ERROR: Write request returns -ENOMEM. Occured due to item \"" << msg << "\".";
            break;
        case -EFAULT:
            cerr << "ERROR: Write request returns -EFAULT. Occured due to item \"" << msg << "\".";
            break;
        default:
            if (status) {
                cerr << "ERROR: Write request returns unknown error. Occured due to item \"" << msg << "\".";
            }
            else {
                cout << "<--- " << msg << " --->";
            }
    }        
    
    cout << " }" << endl;
    return status;
}

int read_test(int &fd) {
    int status = 0;  /* status of read request */
    cout << "READ    { ";
        
    item.size = ITEM_SIZE;
    
    status = ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item); /* send write request */
    switch (status) {
        case -ENOMEM:
            cerr << "ERROR: Read request returns -ENOMEM.";
            break;
        case -EFAULT:
            cerr << "ERROR: Read request returns -EFAULT.";
            break;
        default:
            if (status) {
                cerr << "ERROR: Read request returns unknown error.";
            }
            else {
                cout << "---> " << item.buf << " <---";
            }
    }        
    
    cout << " }" << endl;
    return status;
}

int buffer_size_test(int &fd) {
    int          status = 0;    /* status of buffer size request */
    unsigned int buf_size = 0;  /* size of chdev buffer */
    cout << "BUFSIZE { ";
        
    status = ioctl(fd, CHDEV_IOCTL_GET_BUF_SIZE, &buf_size);
    if (status) {
        cout << "ERROR: Buffer size request failed.";
    }
    else {
        cout << buf_size << " B";
    }
    
    cout << " }" << endl;
    return status;
}

int number_items_test(int &fd) {
    int          status   = 0; /* status of number of items request */
    unsigned int num_item = 0; /* number of items in chdev buffer */
    cout << "NUMITEM { ";
        
        status = ioctl(fd, CHDEV_IOCTL_GET_NUM_ITEM, &num_item);
        if (status) {
            cout << "ERROR: Number of items request failed.";
        }
        else {
            cout << num_item;
        }
        
        cout << " }" << endl;
        return status;
}

void ioctl_test(int &fd) {
    string msg; /* message (item) for chdev */
    
    cout << "--ioctl(...)--" << endl;
    
    /* Buffer size request for /dev/chdev */
    buffer_size_test(fd);
    
    /* Number of items request for /dev/chdev */
    number_items_test(fd);
    
    /* Write requests for /dev/chdev */
    for (int i = 1; i <= 5; i++) {
        msg = "Message #" + to_string(i);
        write_test(msg, fd);
    }
    
    /* Number of items request for /dev/chdev */
    number_items_test(fd);
    
    /* Read requests for /dev/chdev */
    for (int i = 1; i <= 5; i++) {
        read_test(fd);
    }
    
    /* Number of items request for /dev/chdev */
    number_items_test(fd);
    
    cout << endl;
}

void buffer_test(int &fd) {
    struct chdev_item item;         /* used in read and write requests */
    char              buf[100];     /* buffer for read request */
    long              status;       /* that is what ioctl returns */
    
    /* 3 write requests for /dev/chdev */
    string msg;
    cout << "Write requests:" << endl;
    for (int i = 1; i <= 4; i++) {
        msg = "01"; /* header size of item + item == 5B */
        cout << msg << endl;
        item.buf  = const_cast<char *>(msg.c_str());
        item.size = msg.size() + 1; /* +1 because of character with code 0 */
        status = ioctl(fd, CHDEV_IOCTL_SET_ITEM, &item);
        if (status) {
            cout << "/Iteration: " << i << endl
                 << "/Message size: 3B" << endl
                 << "/Status: -ENOMEM" << endl;
        }
    }
    
    /* 1 read request for /dev/chdev */
    item.buf  = buf;
    item.size = 100;
    cout << "Read requests:" << endl;
    memset(item.buf, 0, 100 * sizeof(char));
    if (ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item)) {
        cerr << "ERROR: ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item) returns non zero." << endl;
        exit(EXIT_FAILURE);
    }
    cout << item.buf << endl;
    
    /* 1 write request for /dev/chdev */
    cout << "Write requests:" << endl;
    msg = "01"; /* header size of item + item == 5B */
    cout << msg << endl;
    item.buf  = const_cast<char *>(msg.c_str());
    item.size = msg.size() + 1; /* +1 because of character with code 0 */
    status = ioctl(fd, CHDEV_IOCTL_SET_ITEM, &item);
    if (status) {
        cout << "/Iteration: " << 5 << endl
        << "/Message size: 3B" << endl
        << "/Status: -ENOMEM" << endl;
    }
    
    /* Read requests for /dev/chdev */
    item.buf  = buf;
    item.size = 100;
    cout << "Read requests:" << endl;
    for (int i = 1; i <= 4; i++) {
        memset(item.buf, 0, 100 * sizeof(char));
        if (ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item)) {
            cerr << "ERROR: ioctl(fd, CHDEV_IOCTL_GET_ITEM, &item) returns non zero." << endl;
            exit(EXIT_FAILURE);
        }
        cout << item.buf << endl;
    }
    
    cout << endl;
}

int main() {
    
    int  fd; /* file descriptor */
               
    cout << "CHDEV DRIVER TEST TOOLKIT" << endl 
                                        << endl;
    
    /* /dev/chdev descriptor number */
    cout << "--File descriptor--" << endl;
    fd = open("/dev/chdev", O_RDWR);
    if (fd == -1) {
        cout << "ERROR: /dev/chdev file not found." << endl;
        exit(EXIT_FAILURE);
    }
    else {
        cout << "/dev/chdev: " << fd << endl;
    }
    cout << endl;
    
    /* Tests */
    ioctl_test(fd);
    //buffer_test(fd);
    
    cout << "ALL TESTS PASSED SUCCESSFULLY" << endl;
    
    return EXIT_SUCCESS;
}