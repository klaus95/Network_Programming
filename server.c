/*This file is part of Network_Programming.

    Network_Programming is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Network_Programming is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Network_Programming.  If not, see <http://www.gnu.org/licenses/>.

*/


/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/

#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h>
#include <netdb.h>

void tcpConnection(int, char*);
int isKnown(char*, char*);
int updateFile(char*, int, char* ,char*);
int countDigit(int);
char* itoa(int, char*, int);
int PEER(char *, char *);
int PEERS(int, struct sockaddr_in, char *, int);
int peerInfo(int, char*, char*);
int peerNumber(char*);
void broadcastToPeers(char*, int, char*);
void udpConnection(int, struct sockaddr_in, char*);

void error(char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

void sig_chld(int sig) {
    pid_t pid;
    int store;
    pid = waitpid(sig, &store, 0);
}

int main(int argc, char **argv)
{
    unsigned clilen;
    int sockfd, newsockfd, portno, pid, c, udpfd, maxfdp1, ready;
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;
    char * path;
    fd_set rset;
    char msg[1024];
    bzero(msg, 1024);
    
    while ((c = getopt (argc, argv, "p:d:")) != -1)        //implementing getopt -p and -d
        switch (c)
        {
            case 'p':
                portno = atoi(optarg);
                break;
            case 'd':
                path = optarg;
                break;
            default:
            abort ();
        }
    //TCP listening socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    
    //UDP listening socket
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);
    if (bind(udpfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    signal(SIGCHLD, sig_chld);
    
    FD_ZERO(&rset);
    maxfdp1 = ((sockfd > udpfd) ? (sockfd) : (udpfd)) + 1;
    //printf("sock: %d\nudp: %d\nmax: %d\n", sockfd, udpfd, maxfdp1);
    
    while (1) {
        FD_SET(sockfd, &rset);
        FD_SET(udpfd, &rset);
        if ((ready = select(maxfdp1, &rset, NULL, NULL, NULL)) < 0) {
            if (errno == EINTR) continue;
            else error("select error");
        }
        
        if (FD_ISSET(sockfd, &rset)) {
            //printf("TCP server\n");
            newsockfd = accept(sockfd,
                               (struct sockaddr *) &cli_addr, &clilen);
            if (newsockfd < 0) error("ERROR on accept");
            pid = fork();
            if (pid < 0) error("ERROR on fork");
            if (pid == 0)  {
                close(sockfd);
                tcpConnection(newsockfd, path);
                exit(0);
            }
            else close(newsockfd);
        }
        
        if (FD_ISSET(udpfd, &rset)) {
            //printf("UDP server\n");
            udpConnection(udpfd, cli_addr, path);
        }
    } /* end of while */
    return 0; /* we never get here */
}
//does what it says :P
int removeNewLines(char * str) {
    int len = strlen(str);
    int count = 0;

    int i, shiftRest;
    for (i = 0; i < len; i++) {
        if (str[i] == '\n') {
            count++;
            /*
             *We have to move everything in the remaining string downwards
             *At the core the goal is to turn [a,a,\n,a,\n,\0] into [a,a,a,\0]
             * which is trivially divided into
             * [a,a,a,\n,\0]
             * and into 
             * [a,a,a,\0]
             * and is easy to see that the removal of a \n is really
             * decrementing the position of every element after it
             */ 
            for (shiftRest = i; shiftRest < len; shiftRest++) {
                str[shiftRest] = str[shiftRest+1];
            }
            len--;
            i--;
        }
    }
    str[len] = '\0';
    return count;
}
/*
 * takes: destination buffer, source buffer, length of dest, src buffers
 * returns 0 if successful append
 *         -1 if failure to append(space left in dest is too little for src) 
 */
int bufAppend(char * dest, char * src, int destLen, int srcLen) {
    int destN = strlen(dest);
    int srcN = strlen(src);
    if (destN+srcN+1 > destLen) return -1; //not enough room
    strcat(dest, src);
    return 0;

}
/*
 *return 1 if both time and message are known, 0 else
 */
int isKnownMessageAndTime( char* time, char* message, char* filename) {
    if (access(filename, F_OK) == -1) {return 0;}
    FILE * fgossip;
    fgossip = fopen(filename, "r");
    char line[512];
    char * nullTest;
    bzero(line, 512);
    while (1) {
        nullTest = fgets(line, sizeof line, fgossip);
        if (nullTest == NULL) {
             return 0;
        }
        int messageMatched = 0;
        //printf("before clearing buffer line is %s\n", line);
        if (line[0] == '1') {
            clearBuffer(line, 2, 512);
            //printf("after clearing buffer line is %s\n", line);
            removeNewLines(line);
            messageMatched = strcmp(message, line);
            if (messageMatched) return 0;
            bzero(line, 512);
            fgets(line, sizeof line, fgossip);
            clearBuffer(line, 2, 512);
            removeNewLines(line);
            messageMatched |= strcmp(time, line);
            if (messageMatched == 0) return 1;
            bzero(line, 512);
        }
    }
    return 0;

}

/*
 * takes a buffer with the gossip string
 * returns -1 if the message has already been received
 * returns 0 after broadcasting message
 */
int GOSSIP(char * buf, char * path) {
    //printf("Inside GOSSIP and the string we receive is %s\n", buf);
    char filePath[strlen(path) + 15];
    strcpy(filePath, path);
    strcat(filePath, "fgossip.txt");
    char filePathPeer[strlen(path) + 15];
    strcpy(filePathPeer, path);
    strcat(filePathPeer, "fpeers.txt");
    
    char sha[126];
    char time[64];
    char message[1024];
    bzero(message,1024);
    bzero(time,64);
    bzero(sha,126);
    int bindex = 0, index = 0;
    
    while (buf[bindex] != ':') { bindex++; } //skip GOSSIP:
    bindex++;
    while (buf[bindex] != ':') { sha[index++] = buf[bindex++]; }  //extract sha256 from gossip
    index = 0;
    bindex++;
    while (buf[bindex] != ':') { time[index++] = buf[bindex++]; }  //extract time from gossip
    index = 0;
    bindex++;
    while (buf[bindex] != '%') { message[index++] = buf[bindex++]; }  //extract message from gossip
    //t will contain the line the entry is at
   // if (isKnownMessageAndTime(time, message, filePath)) {
    if (isKnown(message, filePath)) {
        error("DISCARDED");
        return -1;
    } else {
        FILE * fgossip;
        fgossip = fopen(filePath, "a");               //open file to write
        fprintf(fgossip, "BEGIN\n");                     //write header
        fprintf(fgossip, "1:%s\n",message);              //write message
        fprintf(fgossip, "2:%s\n",time);                 //write timestamp
        fprintf(fgossip, "3:%s\n",sha);                  //write sha
        fprintf(fgossip, "END\n");                       //write footer
        
        if (fclose(fgossip)) { error("File not closed properly"); };  //close file
        
        //printf("Inside GOSSIP and before broadcast %s\n", buf);
        int numberOFPeers = peerNumber(filePathPeer);
        //printf("Number of peers: %d\n",numberOFPeers);
        int i;
        for (i = 0; i < numberOFPeers; i++) {
            broadcastToPeers(buf, i, filePathPeer);
        }
        
        error(message);                                 //print message
        return 0;    
    }
}
/*
 * finds the number of peers.
 * returns number of peers.
 * returns -1 in error;
 */
int peerNumber(char * path) {
    char currC;
    int lineNumber = 0;
    
    if (access(path, F_OK) == -1) return -1;
    FILE * finfo;
    finfo = fopen(path, "r");
    
    while (fscanf(finfo,"%c", &currC) == 1) {            //counting number of chars in the file
        if (currC == '\n') lineNumber++;
    }
    int peers = lineNumber/5;
    return peers;
}
/*
 * takes index of a peer and the ip of destination.
 * returns port number.
 * returns -1 in error;
 */
int peerInfo(int peerIndex, char * destination, char * path) {
    bzero(destination, 17);
    char portChar[6];
    bzero(portChar, 6);
    int port, index = 0;
    char currC;
    
    if (access(path, F_OK) == -1) return -1;
    FILE * finfo;
    finfo = fopen(path, "r");
    
    int line = (peerIndex * 5) + 3;
    
    while (fscanf(finfo,"%c", &currC) == 1) {           //read until eof of the old file
        if (currC == '\n') { line--;}                   //count the number of lines
        
        if (line == 1) {                                //found the port line
            fscanf(finfo,"%c", &currC);                 //skiping '\n'
            if (currC == '2') {
                fscanf(finfo,"%c", &currC);             //skip :
                line--;
                while (1) {
                    fscanf(finfo,"%c", &currC);
                    if (currC == '\n'){ break; }
                    portChar[index++] = currC;
                }
            } else {
                error("No the correct line");
                return -1;
            }
        } else if (line == 0) {                          //found ip line
            if (currC == '3') {
                index = 0;
                fscanf(finfo,"%c", &currC);             //skip :
                line--;
                while (1) {
                    fscanf(finfo,"%c", &currC);
                    if (currC == '\n'){ break; }
                    destination[index++] = currC;
                }
            } else {
                error("No the correct line");
                return -1;
            }
        }
    }
    port = atoi(portChar);
    
    if (fclose(finfo)) { error("File not closed properly"); };  //close file
    return port;
}
/* ------------------ TO DO -------------------
 
                    FIX THIS
 
 -------------------------------------------  */
void broadcastToPeers(char * buf, int index, char * path) {
    //printf("%s\n", buf);
    //printf("inside b\n");
    int portno; char hostname[17];
    portno = peerInfo(index, hostname, path);
    int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    
    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    
    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        error("ERROR connecting");
        return;
    }
    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0)
        error("ERROR writing to socket");
    /*
    //printf("Peer No: %d\nPort: %d\nIP: %s\n",index, port, destination);
    struct sockaddr_in cli_addr;
    int udpfd, msglen;
    socklen_t len = sizeof(cli_addr);
    
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) error("ERROR opening socket");
    
    if(sendto(udpfd, buf, strlen(buf), 0, (struct sockaddr *) &cli_addr, len)==-1){ perror("P write"); return; }
    
    close(udpfd);
    */
    close(sockfd);
    //printf("leaving b\n");
}

/*
 * takes a message and checks if the message exists in GOSSIP file.
 * returns the line number were the message was found.
 * returns 0 if it does not find the message.
 */
int isKnown(char* obj, char* filename) {
    int lineNumber = 1;
    if (access(filename, F_OK) == -1) { return 0; }  //test if the file exists
    FILE * fgossip;
    fgossip = fopen(filename, "r");                  //open file
    
    char currC;                                         //holds current char
    int skipFlag = 0, index = 0, objFlag = 0;
    while (fscanf(fgossip,"%c", &currC) == 1) {         //is eof?
        if (!skipFlag) {
            if (!objFlag) {
                if (currC == '1') {                     //line starts with 1? It is a message line.
                    objFlag = 1;
                    fscanf(fgossip,"%c", &currC);       //skiping ':'
                } else {
                    objFlag = 0;
                    skipFlag = 1;                       //line does start with 1? Skip it.
                }
            } else {
                if (obj[index] == '\0' && currC == '\n') {     //end of string? Found it!
                    if (fclose(fgossip)) { error("File not closed properly"); };
                    return lineNumber;
                } else if (currC != obj[index]){        //not a match
                    skipFlag = 1;
                    objFlag = 0;
                } else {                                    //match. Check the next char
                    index++;
                }
            }
        } else {                                         //skip the entire line
            if (currC == '\n'){
                index = 0;
                skipFlag = 0;
                lineNumber++;
            }
        }
    }
    if (fclose(fgossip)) { error("File not closed properly"); };
    return 0;
}

/*
 *   adds a peer to our peer set
 *   returns 0 on successful addition
 *   returns -1 on any failure
 */
int PEER(char * buf, char * path) {
    char name[200];         //tbh no name should be more than 15 or so characters
    char port[6];           //65536\0
    char ip[17];            //nnn.nnn.nnn.nnn\0
    
    bzero(name,200);
    bzero(port,6);
    bzero(ip,17);
    
    char filePath[strlen(path) + 15];
    strcpy(filePath, path);
    strcat(filePath, "fpeers.txt");
    char filePathTemp[strlen(path) + 15];
    strcpy(filePathTemp, path);
    strcat(filePathTemp, "output.txt");
    
    int index = 0;
    while (buf[index] != ':') { index++;} //skip PEER
    int offset = 0;
    index++;
    while (buf[index] != ':') { name[offset++] = buf[index++];}  //extract name from peer
    offset = 0;
    index += 6;
    while (buf[index] != ':') { port[offset++] = buf[index++];}  //extract port from peer
    offset = 0;
    index += 4;
    while (buf[index] != '%') { ip[offset++] = buf[index++]; }  //extract ip from gossip
    
    int lineToUpdate = isKnown(name, filePath);
    if (lineToUpdate) {
        if (updateFile(ip, lineToUpdate, filePath, filePathTemp) == -1) { return -1; }
    } else {
        /*
         * now that data has been gathered into three strings
         * put this data into a file
         *
         */
        
        FILE * fpeers;
        fpeers = fopen(filePath,"a");
        fprintf(fpeers, "BEGIN\n");
        fprintf(fpeers, "1:%s\n", name);
        fprintf(fpeers, "2:%s\n", port);
        fprintf(fpeers, "3:%s\n", ip);
        fprintf(fpeers, "END\n");
        
        if (fclose(fpeers)) { error("File not closed properly"); return -1; }
    }
    return 0;
}
/*
 * updates the address of a peer
 * returns 1 on successful update
 * returns -1 on any error
 */
int updateFile(char* ip, int line, char * peersPath, char * tempPath) {
    int deleteLine = line + 2;              //address line is 2 lines after the name line
    int index = 0;
    char currC;
    
    FILE * ffold;
    ffold = fopen(peersPath, "r");
    FILE * fupdated;
    fupdated = fopen(tempPath, "w");
    
    while (fscanf(ffold,"%c", &currC) == 1) {           //read until eof of the old file
        if (currC == '\n') { deleteLine--; }            //count the number of lines
        
        if (deleteLine == 1) {                          //found the line that will be replaced
            fprintf(fupdated, "\n3:%s\n", ip);          //print the new line
            deleteLine--;
            while (1) {                                 //skip the old line
                fscanf(ffold,"%c", &currC);
                if (currC == '\n'){
                    break;
                }
            }
        } else {
            fprintf(fupdated, "%c", currC);              //copy the unchanged
        }
    }
    
    if (fclose(ffold)) { error("File not closed properly"); return -1; }
    remove(peersPath);                                      //remove the old file
    if (fclose(fupdated)) { error("File not closed properly"); return -1; }
    rename(tempPath, peersPath);                        //rename the new file
    return 1;
}
/*
 * NOTE: writes to socket directly, assumes stdin/out are mapped to socket
 * returns 1 on successful write
 * returns -1 on any error
 */
int PEERS(int sockfd, struct sockaddr_in cli_addr,char * path, int tcpFlag) {
    char filePath[strlen(path) + 15];
    strcpy(filePath, path);
    strcat(filePath, "fpeers.txt");
    
    unsigned len = sizeof(cli_addr);
    char currC;
    char* noPeers = "PEERS|0|%\n";
    int charNumber = 0, lineNumber = 0;
    //printf("We are at line 344\n");
    if (access(filePath, F_OK) == -1) {                  //test if the file exists
        if (tcpFlag) {
            write(sockfd, noPeers, strlen(noPeers));        //sending message
        } else {
            sendto(sockfd, noPeers, strlen(noPeers), 0, (struct sockaddr *) &cli_addr, len);
        }
        return -1;
    }
    FILE * fpeers;
    fpeers = fopen(filePath, "r");
    //printf("We are at line 348\n");
    while (fscanf(fpeers,"%c", &currC) == 1) {            //counting number of chars in the file
        if (currC != '\n') { charNumber++; }
        else { lineNumber++; }
    }
    //printf("We are at line 353\n");
    if (fclose(fpeers)) { error("File not closed properly"); return -1; }
    int peersNumber = lineNumber/5;
    int totalChar = charNumber + 8 - (peersNumber * 14) + (peersNumber * 11) + countDigit(peersNumber);
    //printf("Here we are at line 357\n");
    char message[totalChar + 2];                          //creating char array for the message
    bzero(message,totalChar + 2);
    
    int messageIndex = 0;
    
    char * intro = "PEERS|";
    int i;   
    for (i = 0; i < strlen(intro); i++) {             //adding PEERS| to the message
        message[messageIndex++] = intro[i];
    }
    
    char peerNoArray[countDigit(peersNumber)];            //adding number of peers
    itoa(peersNumber,peerNoArray,10);
    
    for (i = 0; i < strlen(peerNoArray); i++) {
        message[messageIndex++] = peerNoArray[i];
    }
    message[messageIndex++] = '|';
    
    fpeers = fopen(filePath, "r");
    char * port = ":PORT=";
    char * ip = ":IP=";
    
    while (fscanf(fpeers,"%c", &currC) == 1) {            //adding peers to message
        if (currC == 'B' || currC == 'E') {
            while (1) {                                   //skip BEGIN and END lines
                fscanf(fpeers,"%c", &currC);
                if (currC == '\n') {
                    break;
                }
            }
        } else {
            if (currC == '1') {                           //distinuishing name line
                fscanf(fpeers,"%c", &currC);              //skip :
                while (1) {
                    fscanf(fpeers,"%c", &currC);
                    if (currC == '\n') {
                        break;
                    } else {
                        message[messageIndex++] = currC;  //adding name to message
                    }
                }
                int i;
                for (i = 0; i < strlen(port); i++) {  //adding :PORT=
                    message[messageIndex++] = port[i];
                }
            } else if (currC == '2') {                    //distinuishing port line
                fscanf(fpeers,"%c", &currC);              //skip :
                while (1) {
                    fscanf(fpeers,"%c", &currC);
                    if (currC == '\n') {
                        break;
                    } else {
                        message[messageIndex++] = currC; //adding port to message
                    }
                }
                int i;
                for (i = 0; i < strlen(ip); i++) {   //adding :IP=
                    message[messageIndex++] = ip[i];
                }
            } else {                                      //distinuishing ip line
                fscanf(fpeers,"%c", &currC);              //skip :
                while (1) {
                    fscanf(fpeers,"%c", &currC);
                    if (currC == '\n') {
                        break;
                    } else {
                        message[messageIndex++] = currC;  //adding ip to message
                    }
                }
                message[messageIndex++] = '|';
            }
        }
    }
    message[messageIndex++] = '%';                        //adding % to the end of message
    message[messageIndex] = '\n';
    
    if (fclose(fpeers)) { error("File not closed properly"); return -1; }
    
    if (tcpFlag) {
        write(sockfd, message, strlen(message));        //sending message
    } else {
        sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &cli_addr, len);
    }
    
    //printf("%s\n", message);                        //print locally
    return 1;
}
/*
 * counts the number of digits in a int
 */
int countDigit(int n)
{
    int count = 0;
    while (n != 0) {
        n = n / 10;
        ++count;
    }
    return count;
}
/*
 * converts int to char array
 */
char* itoa(int value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }
    
    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );
    
    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

/*
 *To clear processed commands is pretty easy
 *Take everything past that command and shift it down
 *by the size of the command
 * then we have to replace the now copied data with \0
 */
void clearBuffer(char * buffer, int howFar, int bufferSize) {
    int i = 0;
    for (i = howFar; i < bufferSize; i++) {
        buffer[i-howFar] = buffer[i];
        buffer[i] = '\0'; //will get rid of copied strings at the end
                          //that we no longer need
    }
}
//check validity
int commandCount(char * buf) {
    int i, count = 0;
    for (i = 0; i < 1024; i++) {
        if (buf[i] == '%' || buf[i] == '?') {
            //printf("Don't skip\n");
            count++;
        }
    }
    //printf("Skip\n");
    return count;
}
/*
 * Handiling udp connection
 */
void udpConnection(int udpfd, struct sockaddr_in cli_addr, char* path) {
    int n;
    unsigned len = sizeof(cli_addr);
    char msg[1024];
    bzero(msg, 1024);
    n = recvfrom(udpfd, msg, 1024, 0, (struct sockaddr *) &cli_addr, &len);
    if (msg[0] == 'G') {
        GOSSIP(msg, path);
    } else if (msg[4] == ':') {
        PEER(msg, path);
    } else {
        PEERS(udpfd, cli_addr, path, 0);
    }
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void tcpConnection (int sock, char * path){
    struct sockaddr_in empty;
   int n, commands;
   char buffer[1024];  //will be used to hold the concatenated result
   bzero(buffer,1024); 
   char bufTemp[256]; //will be used to hold individual reads
   bzero(bufTemp, 256); 
    while (n = read(sock, bufTemp, 255)) {
        if (n == -1) { error("Error on reading sockets"); }
       int r = bufAppend(buffer, bufTemp, 1024, 256);
       //printf("Number of chars recived %d\n", n);
        //printf("Buffer contains: %s\n", buffer);
        commands = commandCount(buffer);             //counting commands
        while (commands > 0) {                       //dealing with fragmentation and concatination
            removeNewLines(buffer);
            //printf("After removing new lines, buf contains %s\n", buffer);
            /*
             * A command exists to execute if:
             * There exists a string such that it starts with:
             *  G and ends with %
             *  PEER: and ends with %
             *  PEERS and ends with ?
             */
            int index = 0;
            if (buffer[0] == 'G') { //GOSSIP
                for (index = 0; index < 1024; index++) {
                    if (buffer[index] == '%') {
                        char gssp[index + 1];
                        strncpy(gssp, buffer, index + 1);
                        gssp[index + 1] = '\0';
                        //printf("gssp contains %s\n", gssp);
                        GOSSIP(gssp, path);
                        clearBuffer(buffer, index+1,1024);
                        break;
                    }
                }
            } else if (buffer[4] == ':') { //only PEER has this character there
                for (index = 0; index < 1024; index++) {
                    if (buffer[index] == '%') {
                        char per[index + 1];
                        strncpy(per, buffer, index + 1);
                        PEER(per, path);
                        clearBuffer(buffer, index+1,1024);
                        break;
                    }
                }
            } else if (buffer[4] == 'S') {
                //must be peers
                PEERS(sock, empty, path, 1);
                clearBuffer(buffer, 8,1024);
            } else {
                printf("%s", buffer);
            }
            bzero(bufTemp, 256);
            commands--;
        }
        bzero(bufTemp, 256);
   }
}
