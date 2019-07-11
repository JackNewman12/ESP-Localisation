#pragma once

struct MAC80211 {
  // First byte
  unsigned version:2;
  unsigned type:2;
  unsigned subtype:4;

  // Second byte
  unsigned to_DS:1;
  unsigned from_DS:1;
  unsigned more_frag:1;
  unsigned retry:1;
  unsigned power_mgmt:1;
  unsigned more_data:1;
  unsigned wep:1;
  unsigned order:1;

  // Remaining fields
  u8 duration_id[2];
  u8 addr1[6];  //Addr1: Where it's going             https://mrncciew.files.wordpress.com/2014/09/cwap-mac-address-01.png
  u8 addr2[6];  //Addr2: Where it came from
  u8 addr3[6];
  u8 seq_ctrl[2];
  u8 addr4[6];
  u8 qos[2];
  u8 ht[4];

};

// Reference: ESP8266 SDK Programming Guide, section 8.3 Sniffer Structure Introduction
// That programming guide contains the following structures:

struct RxControl { 
  signed rssi:8;             // signal intensity of packet 
  unsigned rate:4; 
  unsigned is_group:1; 
  unsigned:1; 
  unsigned sig_mode:2;       // 0:is 11n packet; 1:is not 11n packet;  
  unsigned legacy_length:12; // if not 11n packet, shows length of packet.  
  unsigned damatch0:1; 
  unsigned damatch1:1; 
  unsigned bssidmatch0:1; 
  unsigned bssidmatch1:1; 
  unsigned MCS:7;            // if is 11n packet, shows the modulation and code used (range from 0 to 76)
  unsigned CWB:1;            // if is 11n packet, shows if is HT40 packet or not  
  unsigned HT_length:16;     // if is 11n packet, shows length of packet. 
  unsigned Smoothing:1; 
  unsigned Not_Sounding:1; 
  unsigned:1; 
  unsigned Aggregation:1; 
  unsigned STBC:2; 
  unsigned FEC_CODING:1;     // if is 11n packet, shows if is LDPC packet or not.  
  unsigned SGI:1; 
  unsigned rxend_state:8; 
  unsigned ampdu_cnt:8; 
  unsigned channel:4;        //which channel this packet in. 
  unsigned:12; 
};

struct LenSeq{ 
    u16 len; // length of packet 
    u16 seq; // serial number of packet, the high 12bits are serial number, low 14 bits are Fragment number (usually be 0)  
    u8 addr3[6]; // the third address in packet  
}; 

struct sniffer_buf { 
  struct RxControl rx_ctrl; 
  u8 buf[36]; // head of ieee80211 packet 
  u16 cnt;    // number count of packet  
  struct LenSeq lenseq[1];  //length of packet  
};

struct sniffer_buf2 { 
  struct RxControl rx_ctrl; 
  u8 buf[112]; 
  u16 cnt;   
  u16 len;  //length of packet  
};


  

