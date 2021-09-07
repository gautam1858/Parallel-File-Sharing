#include <arpa/inet.h>

class host_one {

  public:
    char * hostname;
  char * port;
  char ipstr[INET6_ADDRSTRLEN];
  int file_descriptor;
  bool connection_on;

  double download_st_time;
  host_one & operator = (const host_one b);
};