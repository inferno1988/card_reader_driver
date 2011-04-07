/* 
 * File:   main.c
 * Author: Паламарчук Максим Андрійович
 *
 * Created on December 14, 2010, 11:33 PM
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <microhttpd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "GetCardID.h"

#define NUM_THREADS 512
#define CHAR_SIZE 30

struct MHD_Daemon *crd_daemon;
char *card_id, *page, *host_name, *dev_name;
int PORT = 3425, is_daemon = 0;
pthread_t thread;

static int
answer_to_connection(void *cls, struct MHD_Connection *connection,
        const char *url, const char *method,
        const char *version, const char *upload_data,
        size_t *upload_data_size, void **con_cls) {
    page = card_id;
    struct MHD_Response *response;
    int ret;
    response = MHD_create_response_from_data(strlen(page), (void*) page, MHD_NO, MHD_NO);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    card_id = "func_pull({ \n \"lastCode\": 0, \n \"lastSerial\": 0 \n}) \n";
    MHD_destroy_response(response);
    return ret;
}

void fsignal(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        {
            syslog(LOG_INFO, "Got SIGTERM. exiting!");
            closelog();
            exit(0);
            break;
        }
        case SIGALRM:
        {
            syslog(LOG_INFO, "Got SIGALARM. exiting!");
            closelog();
            exit(0);
            break;
        }
        case SIGQUIT:
        {
            time_t ti_mv;
            struct tm * local_time;
            local_time = localtime(&ti_mv);
            syslog(LOG_INFO, "Card Reader daemon stopped on signal SIGQUIT", sig);
            closelog();
            MHD_stop_daemon(crd_daemon);
            free(card_id);
            free(page);
            free(host_name);
            pthread_exit(&thread);
            exit(0);
            break;
        }
        default:
            syslog(LOG_INFO, "got signal %d. ignore...", sig);
            closelog();
            pthread_exit(&thread);
            exit(0);
            break;
    }
}

void * CardReadThread() {
    int i = 0;
    char * patt1;
    char * patt2;
    char * tmp, *tmp1, *tmp2, *tmp3;
    tmp = (char *) malloc(CHAR_SIZE);
    tmp1 = (char *) malloc(100);
    tmp2 = (char *) malloc(CHAR_SIZE);
    tmp3 = (char *) malloc(CHAR_SIZE);

    for (;;) {
        if (i == 0) {
            tmp = GetCardID(dev_name);
            tmp3 = (char *) memcpy(tmp3, tmp, 15);
            tmp2 = (char *) memchr(tmp, ' ', strlen(tmp));
            tmp2++;
            sprintf(tmp1, "func_pull({ \n \"lastCode\": \"%s\", \n \"lastSerial\": \"%s\" \n}) \n", tmp2, tmp3);
            card_id = tmp1;
            i = 1;
        } else {
            GetCardID(dev_name);
            i = 0;
        }

    }
    free(tmp);
    free(tmp1);
    free(tmp2);
    free(tmp3);
}

int main(int argc, char *argv[]) {
    int rez = 0;
    struct sockaddr_in daemon_ip_addr;
    card_id = (char *) malloc(CHAR_SIZE);
    dev_name = (char *) malloc(100);
    page = (char *) malloc(CHAR_SIZE);
    host_name = (char *) malloc(15);
    dev_name = "/dev/ttyUSB0";
    host_name = "127.0.0.1";
    card_id = "func_pull({ \n \"lastCode\": 0, \n \"lastSerial\": 0 \n}) \n";
    chdir("/");
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    while ((rez = getopt(argc, argv, "p:h:do:")) != -1) {
        switch (rez) {
            case 'p': PORT = atoi(optarg);
                break;
            case 'h': host_name = optarg;
                break;
            case 'd': is_daemon = 1;
                break;
            case 'o': dev_name = optarg;
                break;
            case '?': exit(0);
        };
    };

    if (is_daemon) {
        if (fork())
            exit(0);
        setsid();
    }

    memset(&daemon_ip_addr, 0, sizeof (struct sockaddr_in));
    daemon_ip_addr.sin_family = AF_INET;
    daemon_ip_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, host_name, &daemon_ip_addr.sin_addr);

    int j = 0;
    for (j = 1; j < 32; j++)
        signal(j, fsignal);

    pthread_create(&thread, NULL, CardReadThread, NULL);

    crd_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_SOCK_ADDR, &daemon_ip_addr, MHD_OPTION_END);

    if (crd_daemon == 0) {
        exit(-1);
    }
    syslog(LOG_INFO, "Card Reader daemon started");
    while (1) {
        sleep(60);
    };
}