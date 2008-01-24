#include <utime.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <dirent.h>
#include "job_registry.h"
#include "Bfunctions.h"
#include "config.h"

#define LISTENQ            1024

#ifndef VERSION
#define VERSION            "1.8.0"
#endif

/*  Function declarations  */

int PollDB();
int UpdateFileTime(int sec);
int NotifyStart(char *buffer);
int GetFilter(char *buffer);
int GetModTime(char *filename);
void CreamConnection(int c_sock);
int NotifyCream(char *buffer);

/* Variables initialization */

char *progname="BNotifierCondor";

char *notiffile="/tmp/.notiftime.txt";

char *registry_file;

char *creamfilter="cream_";

int async_notif_port;

int debug=0;
int nodmn=0;

FILE *debuglogfile;
char *debuglogname="/opt/glite/var/log/BNotifierCondor.log";;

struct sockaddr_in cservaddr;

int  list_c;
int  conn_c=-1;
int  c_sock;

/* 
to know if cream is connected:
0 - not connected
1 - connected
*/
int creamisconn=FALSE;
int startnotify=FALSE;
