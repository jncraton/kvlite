/* This is a simple key value store accessible over http.
 * Based off web server code by J. David Blackstone
 * http://sourceforge.net/projects/tinyhttpd/
 */
 
/* The syntax is simple
 * kvlite accepts the following GET requests:
 * /get/[key]
 * /set/key?[value]
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>

#include "md5.h"

#define ISspace(x) isspace((int)(x))

/*
#define SERVER_STRING "Server: kvlite/0.1.0\r\n"
*/
const char * STORE = "/tmp/kvstore/";

//#define ENABLE_LOGGING
#ifdef ENABLE_LOGGING
const char * LOG_FILE = "/tmp/kvlite.log";
#endif

void get(int client, char * key);
void set(int client, char * key, char * value);
void accept_request(int);
void bad_request(int);
void error_die(const char *);
int get_line(int, char *, int);
void headers(int);
void not_found(int);
int startup(u_short *);
void unimplemented(int);

#ifdef ENABLE_LOGGING
int log(char * message);
#endif

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client) {
    char buf[1024];
    int numchars;
    char * value;
    char method[255];
    char url[255];
    size_t i, j;

    /* Parse request method */
    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';

    /* Parse request URL */
    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    /* read & discard headers */
    while ((numchars > 0) && strcmp("\n", buf))  
        numchars = get_line(client, buf, sizeof(buf));

    if( strncasecmp(url,"/get/",5) == 0 ) {
        get(client, url+5);
    } else if ( strncasecmp(url,"/set/",5) == 0 ) {
        value = strchr(url,'?');
        if ( value != NULL ) {
            value[0] = 0x00;
            value++;
            set(client, url+5, value);
        } else {
            not_found(client);
        }
    } else {
        not_found(client);
    }
    
    close(client);
}

void get(int client, char * key) {
    char buf[1024];
    FILE * file;
    char hash[33];
    
    md5(key,hash);
    #ifdef ENABLE_LOGGING
    sprintf(buf,"get value for %s (%s)\n",key,hash);
    log(buf);
    #endif
    
    buf[0] = 0x00;
    strcat(buf, STORE);
    strcat(buf, hash);
    
    file = fopen( buf, "r" );
    if ( file ) {
        headers(client);
        while ( fread(buf,1,1024,file) ) {
            send(client, buf, strlen(buf), 0);
        }
        fclose(file);
    } else {
        not_found(client);
    }}   

void set(int client, char * key, char * value) {
    char buf[1024];
    FILE * file;
    char hash[33];
    
    md5(key,hash);
    #ifdef ENABLE_LOGGING
    sprintf(buf,"set %s (%s) to %s\n",key,hash,value);
    log(buf);
    #endif
    
    buf[0] = 0x00;
    strcat(buf, STORE);
    strcat(buf, hash);
    
    file = fopen( buf, "w" );
    if ( file ) {
        fputs (value,file);
        fclose(file);
        headers(client);
        sprintf(buf, "set %s\n",key);
        send(client, buf, strlen(buf), 0);
    } else {
        #ifdef ENABLE_LOGGING
        sprintf(buf, "not found: %s\n",buf);
        log(buf);
        #endif
        not_found(client);
    }
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc) {
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                 recv(sock, &c, 1, 0);
                else
                 c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
 
    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client) {
    char buf[1024];

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    #ifdef SERVER_STRING
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    #endif
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    #ifdef SERVER_STRING
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    #endif
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>404: Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><h1>404: Not Found</h1>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port) {
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port */ {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
  *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    #ifdef SERVER_STRING
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    #endif
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

#ifdef ENABLE_LOGGING
int log(char * message) {
    static FILE * log_file = NULL;
    char buf[256];
    
    if ( !log_file ) {
        log_file = fopen ( LOG_FILE , "a" );
    }
    
    if ( log_file ) {
        time_t rawtime = time(NULL);
        sprintf(buf,"%s",ctime(&rawtime));
        *strchr(buf, '\n')=0x00;
        fprintf(log_file,"%s - %s\n",buf,message);
        fclose(log_file);
        return 0;
    }
    return -1;
}
#endif 

/**********************************************************************/

int main(int argc, char *argv[]) {
    int server_sock = -1;
    u_short port = 0;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);

    if ( argc != 2 ) {
        printf("Usage: kvlite port\n");
        exit(1);
    } else {
        port = atoi(argv[1]);
    }
    
    server_sock = startup(&port);
    printf("kvlite running on port %d\n", port);
    #ifdef ENABLE_LOGGING
    log("kvlite started");
    #endif
    
    while (1) {
        client_sock = accept(server_sock,
                                   (struct sockaddr *)&client_name,
                                   &client_name_len);
        if (client_sock == -1) error_die("accept");
        #ifdef ENABLE_LOGGING
        log("client connected");
        #endif
        accept_request(client_sock);
    }

    close(server_sock);

    return(0);
}
