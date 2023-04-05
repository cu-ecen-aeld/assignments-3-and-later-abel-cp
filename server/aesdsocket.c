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
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> // inet_ntop
#include <stdio.h>     // printf
#include <string.h>    // memset
#include <stdlib.h>    // exit
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
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
int scanfor(char *buf, char c, size_t limit, size_t *res)
{
  size_t pos = 0;
  char *p;
  
  for (p = buf, pos = 0; *p != c && pos < limit; p++, pos++);

  if (*p == c)
    {
      *res = pos+1;
      return 0;
    }
  return -1;
}


int service(int fd, int logfd)
{
  char *msgbuffer;  // buffer for message store
  char *recvbuf;  // buffer for message store
  char *wptr, *rptr;
  size_t msgbuffer_size = 8192;
  size_t recvbuf_size = 2048;
  size_t nbytes, messagesize, freespace;

  // allocate msg buffer
  msgbuffer = malloc(msgbuffer_size);
  if (msgbuffer == NULL)
    {
      perror("malloc error 1");
      exit(1);
    }

  recvbuf = malloc(recvbuf_size);
  if (recvbuf == NULL)
    {
      perror("malloc error 2");
      exit(1);
    }
  
  messagesize = 0;
  wptr = msgbuffer;
  freespace = msgbuffer_size;
  
  // the loop of receiving
  while(1)
    {
      // try to receive upto 2048 bytes,
      nbytes = recv(fd, recvbuf, recvbuf_size-1, 0);
      if (nbytes < 0) 
	{
	  // recv failed
	  perror("recv error");
	  break;
	}
      if (nbytes == 0)
	{
	  // connection closed
	  break;
	}

      // now nbytes in recvbuf
      // scan received buffer for a pattern, upto nbytes
      size_t position;
      size_t messagecomplete = 0;
      int res;
      res = scanfor(recvbuf, '\n', nbytes, &position);
      if (res == 0)
	{
	  // pattern found
	  messagecomplete = 1;
	}
      // append message to messagebuffer
      memcpy(wptr, recvbuf, position);
      messagesize += position;
      // update wptr
      wptr += position;
      freespace -= position;

      // double buffer size if freespace < recvbuf_size
      if (freespace < recvbuf_size)
	{
	  msgbuffer_size *= 2;
	  if (realloc(msgbuffer, msgbuffer_size) == NULL)
	    {
	      perror("realloc msgbuffer error");
	      free(msgbuffer);
	      break; // exit loop
	    }
	  // update freespace
	  freespace = msgbuffer_size - messagesize;
	}
      // write message buffer to logfd
      if (messagecomplete)
	{
	  if (write(logfd, msgbuffer, messagesize) == -1)
	    {
	      perror("realloc msgbuffer error");
	      free(msgbuffer);
	      break;
	    }
	  // clear message buffer
	  messagesize = 0;
	  wptr = msgbuffer;
	}
    }
  
  free(recvbuf);
  free(msgbuffer);
    
  return 0;
}


int server()
{
  // get a socket for listenning 
  int sfd = get_listener_fd();

  // listen
  if (listen(sfd, 10) != 0)
    {
      perror("listen error");
    }


  // open log file 
  int logfd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
  if (logfd == -1) 
    {
      perror("open error");
      close(sfd);
      exit(1);
    }

  openlog(NULL, LOG_PID|LOG_PERROR, LOG_USER);

  // sigaction
  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    close(sfd);
    close(logfd);
    closelog();
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
      syslog(LOG_DEBUG, "server: got a connection form %s \n", peerhostname);


      // fork a child
      pid_t pid, sid;
      pid = fork();
      if (pid < 0)
	{ // fail
	  close(afd);
	  exit(EXIT_FAILURE);
	}

      if (pid == 0) // fork return 0 in child process
	{
	  close(sfd); // child does not need to listen
	  /* if(send(afd, "Hello from abel", 15, 0) == -1) */
	  /*   { */
	  /*     perror("send"); */
	  /*   } */
	  service(afd, logfd);
	  // child process finished here
	  close(afd);
	  exit(0);
	}

      close(afd);  //parent process does not need this
    }
  
  // close
  close(sfd);
  close(logfd);
  return 0;
}


int main()
{
  return server();
}
