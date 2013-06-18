#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdio>
#include "erl_interface.h"
#include "ei.h"
#include "nodec_common.h"

using namespace nodec;

typedef unsigned char uchar;

struct cnode::cnode_s {
  std::string *host;
  std::string *cookie;
  std::string *name;
  std::string *longName;
  int port;
  int listenSock;
  int epmdSock;
  bool shouldRun;
  bool isRunning;
  pthread_t self;
  struct addrinfo *lookupResults;
  struct in_addr addr;
};

struct cnode::runloop_s {
  int fd;
  cnode::cnode_s *node;
};

static bool ensureFlagNotEmpty(const char *flagname, const std::string &value) {
  return value.length() > 0;
}

static int randomPort() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  srand(tv.tv_sec);
  return 1025 + (rand() % 40000);
}

static inline std::string intToString(int i) {
  std::stringstream ss;
  ss << i;
  return ss.str();
}

static inline std::string etermToString(ETERM *term) {
  FILE *tmp = tmpfile();
  erl_print_term(tmp, term);
  int size = (int) ftell(tmp);
  rewind(tmp);
  char *buf = nodec::mem::calloc<char>(size);
  fread(buf, size, 1, tmp);
  std::string retval(buf);
  std::free(buf);
  return retval;
}

static bool resolveHost(const std::string *host, struct addrinfo **results) {
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  if (getaddrinfo(host->c_str(), NULL, &hints, results) == 0) {
    return true;
  }
  return false;
}

static bool handle_ping(int fd, ETERM *from, ETERM *msg) {
  ETERM *is_auth  = erl_format((char *) "~a", "is_auth");
  ETERM *args = erl_element(3, msg);
  ETERM *arg1 = erl_element(1, args);
  bool retval = false;
  if (erl_match(is_auth, arg1)) {
    ETERM *fromp = erl_element(2, msg);
    ETERM *resp = erl_format((char *) "{~w, yes}", erl_element(2, fromp));
    erl_send(fd, from, resp);
    erl_free_compound(resp);
    retval = true;
    LOG(INFO) << "Responded to net_adm:ping/1";
  }
  erl_free_term(is_auth);
  return retval;
}

static void handle_message(int fd, ErlMessage *emsg) {
  ETERM *gen_call = erl_format((char *) "{~a, _, _}", "$gen_call");
  if (erl_match(gen_call, emsg->msg)) {
    if (!handle_ping(fd, emsg->from, emsg->msg)) {
      LOG(INFO) << "Not a ping";
    }
  }
  else {
    LOG(INFO) << "Not a gen_server call";
  }
}

static void *runConnectionLoop(void *arg) {
  cnode::runloop_s *rl = (cnode::runloop_s *) arg;
  int fd = rl->fd;
  cnode::cnode_s *node = rl->node;
  free(rl);
  while(node->shouldRun) {
    ErlMessage emsg;
    int bufsize = sizeof(uchar) * 2048;
    uchar *buf = (uchar *) malloc(bufsize);
    if (fd != ERL_ERROR) {
      int rc = erl_xreceive_msg(fd, &buf, &bufsize, &emsg);
      if (rc == ERL_ERROR) {
        free(buf);
        break;
      }
      else if (rc == ERL_TICK) {
        free(buf);
        continue;
      }
      else {
        LOG(INFO) << "Received: " << etermToString(emsg.msg);
        handle_message(fd, &emsg);
        free(buf);
        continue;
      }
    }
  }
  return NULL;
}

static void runAcceptLoop(cnode::cnode_s *node) {
  erl_init(NULL, 0);
  struct sockaddr_in *listenAddr = (struct sockaddr_in *)
    node->lookupResults->ai_addr;
  if (erl_connect_xinit((char *) node->name->c_str(), (char *) node->name->c_str(),
                        (char *) node->longName->c_str(),
                        &listenAddr->sin_addr,
                        (char *) node->cookie->c_str(), 0) == -1) {
    LOG(ERROR) << "Error initializing Distributed Erlang";
    return;
  }
  LOG(INFO) << "Distributed Erlang initialized";
  if ((node->epmdSock = erl_publish(node->port)) == -1) {
    LOG(ERROR) << "Error publishing Distributed Erlang port to epmd";
    return;
  }
  LOG(INFO) << "Distributed Erlang port (" << intToString(node->port) << ") published to epmd";
  while(node->shouldRun) {
    ErlConnect conn;
    int fd = erl_accept(node->listenSock, &conn);
    if (fd == ERL_ERROR) {
      node->shouldRun = false;
      break;
    }
    pthread_t thread;
    cnode::runloop_s *rl = nodec::mem::calloc<cnode::runloop_s>(1);
    rl->fd = fd;
    rl->node = node;
    pthread_create(&thread, NULL, runConnectionLoop, (void *) rl);
  }
}

static void *mainLoop(void *arg) {
  cnode::cnode_s *node = (cnode::cnode_s *) arg;
  struct sockaddr_in *listenAddr = (struct sockaddr_in *) node->lookupResults->ai_addr;
  size_t listenAddrLen = node->lookupResults->ai_addrlen;
  if (bind(node->listenSock, (struct sockaddr *) listenAddr, listenAddrLen) < 0) {
    return NULL;
  }
  if (listen(node->listenSock, 100) != 0) {
    LOG(ERROR) << "Unable to begin listening on socket";
    return NULL;
  }
  node->shouldRun = true;
  node->isRunning = true;
  runAcceptLoop(node);
  return NULL;
}

DEFINE_string(node, "", "Erlang node name");
static const bool dummyNode = google::RegisterFlagValidator(&FLAGS_node, &ensureFlagNotEmpty);
DEFINE_string(host, "", "DNS host name or IP address");
static const bool dummyHost = google::RegisterFlagValidator(&FLAGS_host, &ensureFlagNotEmpty);
DEFINE_string(cookie, "", "Distributed Erlang cookie");
static const bool dummyCookie = google::RegisterFlagValidator(&FLAGS_cookie, &ensureFlagNotEmpty);
DEFINE_int32(port, randomPort(), "TCP port used for Distributed Erlang");

cnode::cnode_s *cnode::create(const std::string &host, int port, const std::string &name,
                              const std::string &cookie) {
  cnode::cnode_s *retval =  mem::calloc<cnode::cnode_s>(1);
  if (retval != NULL) {
    std::stringstream longName;
    longName << name << "@" << host;
    retval->host = new std::string(host);
    retval->name = new std::string(name);
    retval->longName = new std::string(longName.str());
    retval->cookie = new std::string(cookie);
    retval->port = port;
  }
  return retval;
}

void cnode::free(cnode::cnode_s **node) {
  if (node == NULL) {
    return;
  }

  cnode::cnode_s *tmp = *node;
  if (tmp != NULL) {
    delete(tmp->host);
    delete(tmp->name);
    delete(tmp->longName);
    delete(tmp->cookie);
    if (tmp->lookupResults != NULL) {
      freeaddrinfo(tmp->lookupResults);
    }
    std::free(tmp);
  }
  *node = NULL;
}

const std::string &cnode::host(const cnode::cnode_s *node) {
  return *(node->host);
}

const std::string &cnode::cookie(const cnode::cnode_s *node) {
  return *(node->cookie);
}

const std::string &cnode::name(const cnode::cnode_s *node) {
  return *(node->name);
}

int cnode::port(const cnode::cnode_s *node) {
  return node->port;
}

bool cnode::isRunning(const cnode::cnode_s *node) {
  return node->isRunning;
}

bool cnode::start(cnode::cnode_s *node) {
  if (resolveHost(node->host, &node->lookupResults)) {
    int on = 1;
    struct sockaddr_in *listenAddr = (struct sockaddr_in *) node->lookupResults->ai_addr;
    node->listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (node->listenSock < 0) {
      return false;
    }
    setsockopt(node->listenSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    listenAddr->sin_port = htons(node->port);
    if (pthread_create(&node->self, NULL, mainLoop, (void *) node) == 0) {
      return true;
    }
  }
  return false;
}

bool cnode::stop(cnode::cnode_s *node, bool waitForStop) {
  return false;
}

void cnode::waitFor(cnode::cnode_s *node) {
  void *ptr;
  pthread_join(node->self, &ptr);
}
