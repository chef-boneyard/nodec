#ifndef CNODE_H
#define CNODE_H

#include "nodec_common.h"

namespace nodec {

  namespace cnode {

    typedef struct cnode_s cnode_s;
    typedef struct runloop_s runloop_s;

    cnode_s *create(const std::string &host, int port, const std::string &name,
                    const std::string &cookie);
    void free(cnode_s **cnode);
    const std::string &host(const cnode_s *cnode);
    const std::string &name(const cnode_s *cnode);
    const std::string &cookie(const cnode_s *cnode);
    int port(const cnode_s *cnode);
    bool isRunning(const cnode_s *cnode);
    bool start(cnode_s *cnode);
    bool stop(cnode_s *cnode, bool waitForStop);
    void waitFor(cnode_s *cnode);
  };
};

DECLARE_string(node);
DECLARE_string(host);
DECLARE_string(cookie);
DECLARE_int32(port);

#endif
