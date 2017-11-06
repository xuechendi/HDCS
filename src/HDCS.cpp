#include "HDCSController.h"

int main(int argc, char **argv) {
  std::string hdcs_name;
  std::string config_name;

  if (argc < 5) {
    printf("usage:\n\t %s --name ${hdcs_config_section_name} --config general.conf\n\ttips: Make sure there is a general.conf under execution dir.\n\n", argv[0]);
    return 1;
  }

  for (int i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      std::string s(argv[i]);
      if (s.compare("--name") == 0) hdcs_name = std::string(argv[i+1]);
      if (s.compare("--config") == 0) config_name = std::string(argv[i+1]);
    }
  }

  hdcs::HDCSController controller(hdcs_name, config_name);
  return 0;
}
