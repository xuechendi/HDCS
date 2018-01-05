#include "ha/HDCSCoreStatController.h"
#include "common/AioCompletionImp.h"
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc == 5) {
  } else {
   printf("Usage:\n\t%s -r server/client -p 11001\n\n", argv[0]);
   return -1;
  }

  std::string cmd;
  std::string node;
  std::shared_ptr<hdcs::ha::HDCSCoreStat> stat;
  if (argv[2][0] == 'c') {
    hdcs::ha::HDCSCoreStatController stat_controller("127.0.0.1", argv[4]);
    while (true) {
      printf("Please input your cmd(add/del/bye/brk): ");
      std::cin >> cmd;
      if (cmd.compare("bye") == 0) break;
      else if(cmd.compare("add") == 0) {
        stat = stat_controller.register_core((void*)0x12345678);
        std::cout << "registered to corestatcontroller.\n";
      } else if (cmd.compare("del") == 0) {
        stat_controller.unregister_core((void*)0x12345678);
        std::cout << "unregistered from heartbeat service.\n";
      } else if (cmd.compare("brk") == 0) {
        stat->update_stat(HDCS_CORE_STAT_ERROR);
      }
    }
  }

  if (argv[2][0] == 's') {
    hdcs::ha::HDCSCoreStatController stat_controller;
    stat_controller.add_listener(argv[4]);
  }
  return 0;
}
