#include "output_ts.h"
#include <mist/http_parser.h>
#include <mist/defines.h>

namespace Mist {
  OutTS::OutTS(Socket::Connection & conn) : TSOutput(conn){
    sendRepeatingHeaders = 500;//PAT/PMT every 500ms (DVB spec)
    streamName = config->getString("streamname");
    parseData = true;
    wantRequest = false;
    pushOut = false;
    initialize();
    std::string tracks = config->getString("tracks");
    if (config->getString("target").size()){
      std::string target = config->getString("target");
      if (target.substr(0,8) != "tsudp://"){
        FAIL_MSG("Target %s must begin with tsudp://, aborting", target.c_str());
        parseData = false;
        myConn.close();
        return;
      }
      //strip beginning off URL
      target.erase(0, 8);
      if (target.find(':') == std::string::npos){
        FAIL_MSG("Target %s must contain a port, aborting", target.c_str());
        parseData = false;
        myConn.close();
        return;
      }
      pushOut = true;
      udpSize = 5;
      sendRepeatingHeaders = true;
      if (target.find('?') != std::string::npos){
        std::map<std::string, std::string> vars;
        HTTP::parseVars(target.substr(target.find('?')+1), vars);
        if (vars.count("tracks")){tracks = vars["tracks"];}
        if (vars.count("pkts")){udpSize = atoi(vars["pkts"].c_str());}
      }
      packetBuffer.reserve(188*udpSize);
      int port = atoi(target.substr(target.find(":") + 1).c_str());
      target.erase(target.find(":"));//strip all after the colon
      pushSock.SetDestination(target, port);
    }
    unsigned int currTrack = 0;
    //loop over tracks, add any found track IDs to selectedTracks
    if (tracks != ""){
      selectedTracks.clear();
      for (unsigned int i = 0; i < tracks.size(); ++i){
        if (tracks[i] >= '0' && tracks[i] <= '9'){
          currTrack = currTrack*10 + (tracks[i] - '0');
        }else{
          if (currTrack > 0){
            selectedTracks.insert(currTrack);
          }
          currTrack = 0;
        }
      }
      if (currTrack > 0){
        selectedTracks.insert(currTrack);
      }
    }
  }
  
  OutTS::~OutTS() {}
  
  void OutTS::init(Util::Config * cfg){
    Output::init(cfg);
    capa["name"] = "TS";
    capa["desc"] = "Enables the raw MPEG Transport Stream protocol over TCP.";
    capa["deps"] = "";
    capa["required"]["streamname"]["name"] = "Stream";
    capa["required"]["streamname"]["help"] = "What streamname to serve. For multiple streams, add this protocol multiple times using different ports.";
    capa["required"]["streamname"]["type"] = "str";
    capa["required"]["streamname"]["option"] = "--stream";
    capa["required"]["streamname"]["short"] = "s";
    capa["optional"]["tracks"]["name"] = "Tracks";
    capa["optional"]["tracks"]["help"] = "The track IDs of the stream that this connector will transmit separated by spaces";
    capa["optional"]["tracks"]["type"] = "str";
    capa["optional"]["tracks"]["option"] = "--tracks";
    capa["optional"]["tracks"]["short"] = "t";
    capa["optional"]["tracks"]["default"] = "";
    capa["codecs"][0u][0u].append("HEVC");
    capa["codecs"][0u][0u].append("H264");
    capa["codecs"][0u][1u].append("AAC");
    capa["codecs"][0u][1u].append("MP3");
    capa["codecs"][0u][1u].append("AC3");
    cfg->addConnectorOptions(8888, capa);
    config = cfg;
    capa["push_urls"].append("tsudp://*");
    
    JSON::Value opt;
    opt["arg"] = "string";
    opt["default"] = "";
    opt["arg_num"] = 1ll;
    opt["help"] = "Target tsudp:// URL to push out towards.";
    cfg->addOption("target", opt);
  }

  void OutTS::sendTS(const char * tsData, unsigned int len){
    if (pushOut){
      static int curFilled = 0;
      if (curFilled == udpSize){
        pushSock.SendNow(packetBuffer);
        packetBuffer.clear();
        packetBuffer.reserve(udpSize * len);
        curFilled = 0;
      }
      packetBuffer.append(tsData, len);
      curFilled ++;
    }else{
      myConn.SendNow(tsData, len);
    }
  }

  bool OutTS::listenMode(){
    return !(config->getString("target").size());
  }

}
