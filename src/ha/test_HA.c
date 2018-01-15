#include "ha/HAClient.h"
#include "ha/HAManager.h"
#include "common/AioCompletionImp.h"
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc == 3) {
  } else {
   printf("Usage:\n\t%s -r server/client\n\n", argv[0]);
   return -1;
  }

  std::string cmd;
  std::string node;
  if (argv[2][0] == 'c') {
    hdcs::ha::HAClient ha_client;
    std::shared_ptr<hdcs::ha::HDCSCoreStat> stat;
    while (true) {
      printf("Please input your cmd(connect/add/bye/brk): ");
      std::cin >> cmd;
      if (cmd.compare("bye") == 0) break;
      else if(cmd.compare("connect") == 0) {
        printf("Please input server ip and port(ex:127.0.0.1:10001) : ");
        std::cin >> cmd;
        ha_client.add_ha_server(cmd);
      }else if(cmd.compare("add") == 0) {
        printf("Registered core id is 0x12345678\n");
        stat = ha_client.register_core((void*)0x12345678);
      } else if (cmd.compare("brk") == 0) {
        stat->update_stat(HDCS_CORE_STAT_ERROR);
      }
    }
  }

  if (argv[2][0] == 's') {
    hdcs::ha::HAManager ha_mgr;
    while (true) {
      printf("Please input your cmd(connect/bye): ");
      std::cin >> cmd;
      if (cmd.compare("bye") == 0) break;
      else if(cmd.compare("connect") == 0) {
        printf("Please input server ip and port(ex:127.0.0.1:11001) : ");
        std::cin >> cmd;
        ha_mgr.register_hdcs_node(cmd);
      }
    }
  }
  return 0;
}
