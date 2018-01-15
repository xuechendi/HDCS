#ifndef HDCS_HA_CONFIG
#define HDCS_HA_CONFIG

namespace hdcs {
namespace ha {
class HAConfig {
public:
  HAConfig (std::string path) {

  }

  ~HAConfig () {

  }

  std::string get(std::string key) {
    return config_data.find(key)->second;
  }
private:
  std::map<std::string, std::string> config_data{
    {"ha_heartbeat_listen_port", "10000"}
  };
};
}// ha
}// hdcs

#endif
