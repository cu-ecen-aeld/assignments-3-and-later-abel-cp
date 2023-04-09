/*
  1. stream socket
  2. bind to port 9000
  3. listen and accept forever until SIGINT or SIGTERM received
  4. log accepted connection
  5. log data
  6. send recieved packet back to client
  7. log message to syslog when connection closed

  some notes:
  big endian: big end fisrt. big end stores at lower address
  network byte order, --- ntoh --> host byte order
                      <-- hron ---

  Primative: system calls

  struct addrinfo {
  int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
  int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
  int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
  int              ai_protocol;  // use 0 for "any"
  size_t           ai_addrlen;   // size of ai_addr in bytes
  struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
  char            *ai_canonname; // full canonical hostname
  struct addrinfo *ai_next;      // linked list, next node
  };


 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h> // inet_ntop
#include <stdio.h>     // printf
#include <string.h>    // memset
#include <stdlib.h>    // exit
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>


void showipinfo(const struct addrinfo *p)
{
  void *addr;
  char ipstr[INET6_ADDRSTRLEN];


  if (p->ai_family == AF_INET) {
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
    addr = &(ipv4->sin_addr);
  }
  else {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    addr = &(ipv6->sin6_addr);
  }

  // convert the IP to a string and print it:
  inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
  printf("  %s: %s\n", p->ai_canonname, ipstr);  
}

void walkaddrinfo(struct addrinfo *servinfo)
{
  struct addrinfo *p;
  int i;
  for (p=servinfo, i=0; p!=NULL; p=p->ai_next, i++)
    {
      // print ai_family, ip, port, host name
      printf("%d", i);
      showipinfo(p);
    }
}

int trytobind(int sfd, struct addrinfo *p)
{
  int status;
  
  if ((status = bind(sfd, p->ai_addr, p->ai_addrlen)) == -1)
    {
      // error
      perror("bind error");
    }
  else
    {
      printf("bind success\n");
      showipinfo(p);
    }
  return status; // sucess
}

void sigchld_handler()
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);
    syslog(LOG_DEBUG, "sigchld_handler");
    errno = saved_errno;
}


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
  would need an argument for PORT, 
  but the assignment has port fixe at 9000, so skip the argument for now
  return -1 if fail else return file descriptor
  
 */
int get_listener_fd()
{
  /* getaddrinfo; */
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;  // will point to the results

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

  // port is the service we are providing so we know that
  if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
    /* fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status)); */
    perror("getaddrinfo error");
    exit(1);
  }

  /* go through the addrinfo list and try to get a socket descriptor */
  int sfd;
  struct addrinfo *p;
  int yes = 1;
  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      // create a socket
      sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sfd == -1) // error
	{
	  perror("socket error");
	  continue; // try next addr
	}
      
      if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
	{
	  perror("setsockopt");
	  close(sfd);
	  freeaddrinfo(servinfo);
	  exit(1);
	}
      
      if (trytobind(sfd, p) == -1)
	{
	  // bind failed
	  close(sfd);
	  continue;
	}
      break;
    }

  // after binding, we don't need servinfo anymore
  freeaddrinfo(servinfo);
  
  // check if bind was successful
  if (p == NULL)
    {
      fprintf(stderr, "server: failed to bind\n");
      exit(1);
    }

  /* return */
  return sfd;
}

// res is the length of substr that ends with c or limit if c not found
int scanfor(char *buf, char c, size_t limit, size_t *pos)
{
  int res = 0;
  size_t index = 0;

  for (; *buf != c && index < limit; buf++, index++);
  *pos = index;
  
  if (*buf == c)
    {
      res =  0;
    }

  if (*pos == limit)
    {
      res = 1;
    }
  syslog(LOG_DEBUG, "*buf = %02x, index = %ld, return value = %d", *buf, index, res);
  return res;
}

/* send all message in logfd, though fd */
int send_all(int fd, int logfd, char *buf, size_t buf_size)
{
  // read from logfd
  size_t bytesread=1;

  lseek(logfd, 0, SEEK_SET);
  while (bytesread > 0)
    {
      bytesread = read(logfd, buf, buf_size);
      if (bytesread==-1)
	{
	  perror("read error");
	  syslog(LOG_DEBUG, "error in reading data log");
	  return -1;
	}
      if (bytesread == 0)
	return 0;

      syslog(LOG_DEBUG,"sending %ld bytes back to client", bytesread);
      if(send(fd, buf, bytesread, 0) == -1)
	{
	  perror("send error");
	  syslog(LOG_DEBUG, "error sending data back");
	}
    }
}


int service(int fd, int logfd)
{
  //  char *msgbuffer;  // buffer for message store
  char *recvbuf;  // receiving buffer
  char *sendbuf;  // sending buffer
  char *wptr, *rptr;
  //  size_t msgbuffer_size = 8192;
  size_t recvbuf_size = 2048;
  size_t sendbuf_size = 2048;
  size_t nbytes, messagesize, freespace;

  size_t position;
  size_t messagecomplete = 0;
  int res;

  // allocate msg buffer
  /* msgbuffer = malloc(msgbuffer_size); */
  /* if (msgbuffer == NULL) */
  /*   { */
  /*     perror("malloc error 1"); */
  /*     exit(1); */
  /*   } */
  /* syslog(LOG_DEBUG, "msgbuffer pointer = %p", msgbuffer); */

  recvbuf = malloc(recvbuf_size);
  if (recvbuf == NULL)
    {
      perror("malloc error 2");
      exit(1);
    }
  syslog(LOG_DEBUG, "recvbuf pointer = %p", recvbuf);
  
  sendbuf = malloc(sendbuf_size);
  if (recvbuf == NULL)
    {
      perror("malloc error 3");
      exit(1);
    }
  syslog(LOG_DEBUG, "sendbuf pointer = %p", sendbuf);
  
  /* messagesize = 0; */
  /* wptr = msgbuffer; */
  /* freespace = msgbuffer_size; */
  
  // the loop of receiving
  while(1)
    {
      // try to receive upto 2048 bytes,
      nbytes = recv(fd, recvbuf, recvbuf_size, 0);
      syslog(LOG_DEBUG, "recv returned %ld bytes", nbytes);
      
      if (nbytes < 0) 
	{
	  // recv failed
	  perror("recv error");
	  break;
	}
      if (nbytes == 0)
	{
	  // connection closed
	  syslog(LOG_DEBUG, "-- connection closed");
	  break;
	}

      // write to logfd2
      // syslog(LOG_DEBUG,"write recvbuf to my log file");
      /* if (write(logfd2, recvbuf, nbytes) == -1) */
      /* 	{ */
      /* 	  perror("write logfd2 error"); */
      /* 	  syslog(LOG_DEBUG,"write recvbuf to my log file ERROR"); */
      /* 	  break; */
      /* 	} */

      // now nbytes in recvbuf
      // scan received buffer for a pattern, upto nbytes
      res = scanfor(recvbuf, '\n', nbytes, &position);
      if (res == 0)
	{
	  position++;
	}
      // syslog(LOG_DEBUG,"nbytes = %ld, res = %d, position = %ld, messagesize = %ld, freespace = %ld", nbytes,res, position, messagesize, freespace);
      // append message to messagebuffer
      /* memcpy(wptr, recvbuf, position); */
      /* messagesize += position; */
      // update wptr
      /* wptr += position; */
      /* freespace -= position; */
      // syslog(LOG_DEBUG,"after memcpy, nbytes = %ld, res = %d, position = %ld, messagesize = %ld, freespace = %ld", nbytes,res, position, messagesize, freespace);
      // double buffer size if freespace < recvbuf_size
      /* if (freespace <= recvbuf_size) */
      /* 	{ */
      /* 	  msgbuffer_size *= 2; */
      /* 	  syslog(LOG_DEBUG, "freespace = %ld, calling realloc with new buffer size = %ld", freespace, msgbuffer_size); */
	  
      /* 	  msgbuffer = (char *)realloc((void *)msgbuffer, msgbuffer_size); */
      /* 	  if (msgbuffer  == NULL) */
      /* 	    { */
      /* 	      syslog(LOG_DEBUG, "realloc msgbuffer error!!"); */
      /* 	      perror("realloc msgbuffer error"); */
      /* 	      break; // exit loop */
      /* 	    } */

      /* 	  syslog(LOG_DEBUG, "new pointer = %p", msgbuffer); */
      /* 	  // update freespace */
      /* 	  freespace = msgbuffer_size - messagesize; */
      /* 	  // syslog(LOG_DEBUG, "new freespace = %ld", freespace); */

      /* 	} */
      // write message buffer to logfd
      syslog(LOG_DEBUG, "write %ld bytes to file", position);
      if (write(logfd, recvbuf, position) == -1)
	{
	  perror("write message to file error");
	  syslog(LOG_DEBUG,"write message to file error");
	  break;
	}

      if (res == 0)
	{
	  // syslog(LOG_DEBUG, "write %ld bytes to file", messagesize);
	  /* syslog(LOG_DEBUG, "write %ld bytes to file", position); */
	  /* if (write(logfd, recvbuf, position) == -1) */
	  /*   { */
	  /*     perror("write message to file error"); */
	  /*     syslog(LOG_DEBUG,"write message to file error"); */
	  /*     break; */
	  /*   } */
	  /* if (write(logfd, msgbuffer, messagesize) == -1) */
	  /*   { */
	  /*     perror("write message to file error"); */
	  /*     syslog(LOG_DEBUG,"write message to file error"); */
	  /*     break; */
	  /*   } */
	  // clear message buffer
	  /* messagesize = 0; */
	  /* wptr = msgbuffer; */

	  // copy anything after pattern to msgbuffer;
	  /* if (position < nbytes-1) */
	  /*   { */
	  /*     memcpy(wptr, recvbuf+position, nbytes-1-position); */
	  /*     wptr += nbytes-1-position; */
	  /*   } */
	  // send all received message back
	  if (send_all(fd, logfd, sendbuf, sendbuf_size) == -1)
	    {
	      //break;
	    }
	  // write the rest
	  if (position < nbytes)
	    {
	      if (write(logfd, recvbuf+position, nbytes-position) == -1)
		{
		  perror("write message to file error");
		  syslog(LOG_DEBUG,"write message to file error");
		  break;
		}
	    }
	}
    }
  
  syslog(LOG_DEBUG, "remving aesdsocketdata file");
  unlink("/var/tmp/aesdsocketdata");
  if (sendbuf != NULL)
    {
      syslog(LOG_DEBUG, "freeing sendbuf %p ", sendbuf);
      free(sendbuf);
    }
  if (recvbuf != NULL)
    {
      syslog(LOG_DEBUG, "freeing recvbuf %p ", recvbuf);
      free(recvbuf);
    }
  /* if (msgbuffer != NULL) */
  /*   { */
  /*     syslog(LOG_DEBUG, "freeing msgbuffer %p", msgbuffer); */
  /*     free(msgbuffer); */
  /*   } */
  
  return nbytes;
}


int server(int daemon_mode)
{
  // get a socket for listenning 
  int sfd = get_listener_fd();
  pid_t pid, sid;

  // daemonize
  if (daemon_mode)
    {
      pid = fork();
      if (pid < 0)
	{ // fail
	  close(sfd);
	  exit(EXIT_FAILURE);
	}

      if (pid > 0)
	{
	  // parent process
	  exit(EXIT_SUCCESS);
	}

      // change the file mode mask
      umask(0);

      // create a new SID for the child process
      sid = setsid();
      if (sid < 0)
	{
	  perror("setsid error");
	  exit(EXIT_FAILURE);
	}
      
       /* Change the current working directory */
      if ((chdir("/")) < 0)
	{
	/* Log the failure */
	exit(EXIT_FAILURE);
	}
        
      /* Close out the standard file descriptors */
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
        
      /* Daemon-specific initialization goes here */
    }
  // open log file 
  int logfd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
  if (logfd == -1) 
    {
      perror("open error");
      close(sfd);
      exit(1);
    }

  // create a new sid for the child process
  
  // listen
  if (listen(sfd, 10) != 0)
    {
      perror("listen error");
    }



  /* int logfd2 = open("/var/tmp/mylog", O_RDWR|O_CREAT, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH); */
  /* if (logfd2 == -1)  */
  /*   { */
  /*     perror("open error"); */
  /*     close(sfd); */
  /*     exit(1); */
  /*   } */

  openlog(NULL, LOG_PID|LOG_PERROR, LOG_USER);

  // sigaction
  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    syslog(LOG_DEBUG, "Caught signal, existing");
    close(sfd);
    close(logfd);
    //    close(logfd2);
    closelog();
    unlink("/var/tmp/aesdsocketdata");
    unlink("/var/tmp/mylog");
    exit(1);
  }
  
  // accept loop
  int afd;
  socklen_t addr_size;
  struct sockaddr_storage peer_addr;
  char peerhostname[INET6_ADDRSTRLEN];
  while(1)
    {
      addr_size = sizeof peer_addr;
      afd = accept(sfd, (struct sockaddr *) &peer_addr, &addr_size);
      if (afd == -1)
	{
	  perror("accept error");
	  continue;
	}

      inet_ntop(peer_addr.ss_family,
		get_in_addr((struct sockaddr *) &peer_addr),
		peerhostname,
		sizeof(peerhostname));
      syslog(LOG_DEBUG, "Accepted connection form %s \n", peerhostname);


      // fork a child
      //pid_t pid;
      pid = fork();
      if (pid < 0)
	{ // fail
	  close(afd);
	  exit(EXIT_FAILURE);
	}

      if (pid == 0) // fork return 0 in child process
	{
	  size_t n;
	  close(sfd); // child does not need to listen

	  //n = service(afd, logfd, logfd2);
	  n = service(afd, logfd);
	  syslog(LOG_DEBUG, "service returned value = %ld\n", n);
	  if(n == 0)
	    {
	      syslog(LOG_DEBUG, "Closed connection form %s \n", peerhostname);
	    }
	  else
	    {
	    perror("service");
	    }
	  // child process finished here
	  close(afd);
	  exit(0);
	}

      close(afd);  //parent process does not need this
    }
  
  // close
  close(sfd);
  close(logfd);
  //close(logfd2);
  if (daemon_mode == 1)
    {
      exit(EXIT_SUCCESS);
    }
  return 0;
}


int main(int argc, char **argv)
{
  int daemon_mode = 0;

  int c;
  while ((c = getopt (argc, argv, "d")) != -1)
    {
      if (c == 'd')
	{
	  daemon_mode = 1;
	  break;
	}
    }
  return server(daemon_mode);
}
