#include "nodec_common.h"

static void initEnvironment(int *argc, char ***argv) {
  google::ParseCommandLineFlags(argc, argv, true);
  google::InitGoogleLogging(*argv[0]);
}

int main(int argc, char **argv) {
  initEnvironment(&argc, &argv);
  LOG(INFO) << "nodec starting up...";
  LOG(INFO) << "Long node name: " << FLAGS_node << "@" << FLAGS_host;
  nodec::cnode::cnode_s *node = nodec::cnode::create(FLAGS_host,
                                                     FLAGS_port,
                                                     FLAGS_node,
                                                     FLAGS_cookie);
  if (node == NULL) {
    goto ERROR;
  }
  if (nodec::cnode::start(node) == false) {
    return 1;
  }
  nodec::cnode::waitFor(node);
  return 0;

ERROR:
  LOG(ERROR) << "Errors occurred during startup. Exiting...";
  if (node != NULL) {
    nodec::cnode::free(&node);
  }
  return 1;
}
