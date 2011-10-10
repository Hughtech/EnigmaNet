/*
Copyright (C) 2010 IsmAvatar <IsmAvatar@gmail.com>

This file is part of EnigmaNet
EnigmaNet is free software and comes with ABSOLUTELY NO WARRANTY.
See LICENSE for details.
*/

// Implementation of some major aspects of protocols for http and ftp.
// This allows easy communication with FTP and HTTP servers.
// An example is also provided in the main() method at the bottom.

#include "net.h"
#include <stdarg.h>
#include <stdlib.h>

/*
The following resources may be handy in writing your http client/server:
Http made really easy
http://www.jmarshall.com/easy/http/#structure
W3C HTTP/1.1 (especially helpful with understanding headers)
http://www.w3.org/Protocols/rfc2616/rfc2616.html
RFC 2616 (very difficult to read)
http://www.ietf.org/rfc/rfc2616.txt
*/

void die(char *e, int n, ...) {
 printf("%s error (%d)\n",e,errno);
 int i;
 va_list s;
 va_start(s,n);
 for (i = 0; i < n; ++i)
  closesocket(va_arg(s,int));
 va_end(s);
 exit(1);
}

int prepare(char **ret, const char *cmd, int argc, ...) {
 char *str;
 int i, len = strlen(cmd);
 va_list vl;
 //get lengths of args minus %s
 va_start(vl,argc);
 for (i = 0; i < argc; ++i) {
  str = va_arg(vl,char*);
  len += strlen(str) - 2;
 }
 va_end(vl);
 //allocate
 *ret = (char *)malloc(len);
 //print
 va_start(vl,argc);
 vsprintf(*ret,cmd,vl);
 va_end(vl);
 return len;
}

void ftpsend(int in, char *msg) {
 printf("> %s",msg);
 if (net_send(in,msg,strlen(msg)) < 0)
  die("Send",1,in);
}

void ftpparse(int in, char *cmd, char *arg, char *disp) {
 int len = strlen(cmd) + strlen(arg) - 2;
 char *packet = (char *)malloc(len);
 sprintf(packet,cmd,arg);
 printf("> ");
 printf(cmd,disp);
 len = net_send(in,packet,len);
 free(packet);
 if (len < 0) die("Send",1,in);
}

char *ftpexpect(int in, char *exp) {
 char *packet = net_receive(in);
 printf("%s",packet);
 if (strncmp(packet,exp,4) != 0) die("Response",1,in);
 return packet;
}

//Opens a connection with an FTP server using given hostname, username, and password.
//Returns an identifier for the FTP connection.
int ftp_open(char *host, char *user, char *pass) {
 int in = net_connect_tcp(host,"ftp",0);
 if (in < 0) die("Connect",0);
 ftpexpect(in,"220 ");

 ftpparse(in,"USER %s\r\n",user,user);
 ftpexpect(in,"331 ");

 ftpparse(in,"PASS %s\r\n",pass,"<omitted>");
 ftpexpect(in,"230 ");
 
 return in;
}

//Uploads data to an ftp server to be stored as file.
//Indicate the ftp connection, the remote filename to store the data in, and the data (and size)
void ftp_send(int in, char *file, char *msg, int msglen) {
 ftpsend(in,"PASV\r\n");
 char *ip = ftpexpect(in,"227 ");

 //get passive IP and port
 //note, IP resides in buffer, do not overwrite until finished
 ip = strchr(ip,'(') + 1;
 char *r2 = strchr(ip,',');
 char p,port[6];
 for (p = 0; p < 3; ++p) {
  *r2 = '.';
  r2 = strchr(r2,',');
 }
 *(r2++) = '\0';
 sprintf(port,"%d\0",atoi(r2) * 256 + atoi(strchr(r2,',') + 1));
 int out = net_init_tcp(ip,port,0);

 ftpparse(in,"STOR %s\r\n",file,file); //ip overwritten
 ftpexpect(in,"150 ");
 net_send(out,msg,msglen);
 closesocket(out);
 ftpexpect(in,"226 ");
}

//Properly terminates the connection with this ftp socket.
void ftp_close(int in) {
 ftpsend(in,"QUIT\r\n");
 ftpexpect(in,"221 ");
 closesocket(in);
}

//Queries a http host and location, returns the contents
//This method handles the packets and headers for you
char *http(char *host, char *loc) {
 char *packet;

 int s = net_init_tcp(host,"http",0);
 if (s < 0) die("Connect",0);

 char *cmd = "\
GET %s HTTP/1.1\r\n\
Host: %s\r\n\
Connection: close\r\n\r\n";
 int len = prepare(&packet,cmd,2,loc,host);
 int r = net_send(s,packet,len);
 free(packet);
 if (r < 0) die("Send",1,s);

 if ((packet = net_receive(s)) == NULL)
  die("Receive",1,s);

 //HTTP 1.1 requires handling of 100 Continue.
 while (strstr(packet,"HTTP/1.1 100 Continue\r\n") != NULL)
  packet = strstr(packet,"\r\n\r\n") + 4;
 //skip the header
 packet = strstr(packet,"\r\n\r\n") + 4;

 closesocket(s);
 return packet;
}

//An example which fetches your external IP from http website 'whatismyip'.
//and then uploads that IP an ftp server (here we assume localhost with a no-password root).
//The ip gets stored on the ftp server as "ip.txt".
int main() {
 char *ip = http("www.whatismyip.com","/automation/n09230945.asp");
 printf("Your IP is %s\n\n",ip);

 char *host = "localhost";
 char *user = "root";
 char *pass = "";
 int ftp = ftp_open(host,user,pass);
 ftp_send(ftp,"ip.txt",ip,strlen(ip));
 ftp_close(ftp);
}

