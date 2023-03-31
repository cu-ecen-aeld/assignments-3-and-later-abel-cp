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
#include <unistd.h>
#include <sys/wait.h>

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


int main()
{

  // for the server, we use
  // socket(int domain, int type, int protocol) to create a socket fd
  // then bind sfd with address.. but we don't know the ip of the machine
  // so getaddrinfo comes to the rescue.
  
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;  // will point to the results
  socklen_t addr_size;
  struct sockaddr_storage peer_addr;
  char hostname[256];
  struct sigaction sa;
  char peerhostname[INET6_ADDRSTRLEN];

  
  gethostname(hostname, sizeof(hostname));
  printf("%s\n", hostname);
  
  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

  // port is the service we are providing so we know that
  if ((status = getaddrinfo(NULL, "3010", &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  // servinfo now points to a linked list of 1 or more struct addrinfos
  // ... do everything until you don't need servinfo anymore ....
  // if more than 1 addrinfos which one to use?
  walkaddrinfo(servinfo);

  printf("getting socket file descriptor\n");
  
  // create a socket has nothing to do with servinfo 
  int sfd, afd;
  struct addrinfo *p;
  int yes = 1;

  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      // create a socket
      sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sfd == -1) // error
	{
	  perror("socket error");
	  continue;
	}
      
      if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes,
		     sizeof(int)) == -1) {
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
   
  // listen
  printf("listening\n");
  
  if (listen(sfd, 10) != 0)
    {
      perror("listen error");
    }

  // sigaction
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  // accept loop
  while(1)
    {
      addr_size = sizeof peer_addr;
      afd = accept(sfd, (struct sockaddr *) &peer_addr, &addr_size);
      if (afd == -1)
	{
	  perror("accept ---");
	  continue;
	}

      inet_ntop(peer_addr.ss_family,
		get_in_addr((struct sockaddr *) &peer_addr),
		peerhostname,
		sizeof(peerhostname));
      printf("server: got a connection form %s \n", peerhostname);


      // fork a child
      if (!fork()) // fork return 0 in child process
	{
	  close(sfd); // child does not need to listen
	  if(send(afd, "Hello from abel", 15, 0) == -1)
	    {
	      perror("send");
	    }

	  // child process finished here
	  close(afd);
	  exit(0);
	}
      close(afd);  //parent process does not need this
    }
  
  // close
  close(sfd);
  return 0;
}
