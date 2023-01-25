#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
/* write.c
   parameter1 writefile
   parameter2 writestr
   
   return 1 if missing arguments
   return 1 and print error message if fail to create file
*/

int writer(const char *writefile, const char *writestr)
{
  int fd;
  fd = open(writefile, O_WRONLY |O_TRUNC | O_CREAT, 0644);
  if (fd == -1) {
    perror("can not open file");
    return 1;
  }

  write(fd, writestr, strlen(writestr));
  
  //syslog(LOG_USER | LOG_DEBUG, "Writing %s to %s", "writestr", "writefile");
  close(fd);
  return 0;
}

int main(int argc, char **argv)
{
  openlog(NULL, LOG_CONS, LOG_USER);
  if (argc < 2) {
    syslog(LOG_USER, "%s", "missing writefile and writestr");
    return 1;
  }
  
  else if (argc < 3) {
    syslog(LOG_USER, "%s", "missing 1 of 2 required arguments");
    return 1;
  }
  else if (argc > 3) {
    syslog(LOG_USER, "%s", "too many arguments");
    return 1;
  }

  writer(argv[1], argv[2]);
  closelog();
  return 0;
}
