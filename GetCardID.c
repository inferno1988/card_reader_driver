/*
 * File:   GetCardID.c
 * Author: Паламарчук Максим Андрійович
 *
 * Created on December 17, 2010, 11:03 AM
 */

#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include "GetCardID.h"

#define BAUDRATE B9600
#define _POSIX_SOURCE 1         //POSIX compliant source
#define FALSE 0
#define TRUE 1
#define CHAR_SIZE 30

int wait_flag = 1;

char * GetCardID(char *devicename) {
    FILE *out;
    size_t size;
    char *ptr;
    int fd = 0, res = 0, i = 0, s = 0;
    char In1;
    void signal_handler_IO(int status);
    int status;
    struct termios oldtio, newtio;
    struct sigaction saio; //definition of signal action
    char *buf;
    buf = (char *) malloc(100);
    out = open_memstream(&ptr, &size);
    if (out == NULL) {
        perror("fmemopen");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_ALERT, "%s", devicename);
    closelog();
    fd = open(devicename, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        syslog(LOG_ALERT, "%s", strerror(errno));
        closelog();
        exit(0);
    }

    saio.sa_handler = signal_handler_IO;
    sigemptyset(&saio.sa_mask); //saio.sa_mask = 0;
    saio.sa_flags = 0;
    saio.sa_restorer = NULL;
    sigaction(SIGIO, &saio, NULL);

    // allow the process to receive SIGIO
    fcntl(fd, F_SETOWN, getpid());
    // Make the file descriptor asynchronous (the manual page says only
    // O_APPEND and O_NONBLOCK, will work with F_SETFL...)
    fcntl(fd, F_SETFL, FASYNC);

    tcgetattr(fd, &oldtio);

    newtio.c_cflag = B9600 | CRTSCTS | CS8 | 0 | 0 | 0 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0; //ICANON;
    newtio.c_cc[VMIN] = 1;
    newtio.c_cc[VTIME] = 0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);
    int j = 0;
    char str[80];

    while (1) {
        res = read(fd, buf, sizeof (buf));

        if (res == -1) {
            syslog(LOG_ALERT, "%s", strerror(errno));
            closelog();
            exit(0);
        }

        for (i = 0; i < res; i++) {
            In1 = buf[i];
            if (In1 == 0x0d) {
                break;
            };
            s = fprintf(out, "%c", In1);
            if (s == -1) {
                syslog(LOG_ALERT, "%s", strerror(errno));
                closelog();
                exit(0);
            }
            fflush(out);
        }

        wait_flag = TRUE;
        if (In1 == 0x0d) {
            break;
        };
    
    }

    tcsetattr(fd, TCSANOW, &oldtio);
    free(buf);
    fclose(out);
    close(fd);
    return ptr;
}

void signal_handler_IO(int status) {
    wait_flag = FALSE;
}