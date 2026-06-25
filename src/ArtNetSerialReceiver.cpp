#include "ArtNetSerialReceiver.h"

const uint8_t ArtNetSerialReceiver::ARTNET_ID[ArtNetSerialReceiver::ARTNET_HEADER_SIZE] = {
  'A', 'r', 't', '-', 'N', 'e', 't', 0x00
};


ArtNetSerialReceiver::ArtNetSerialReceiver(PacketCallback pktCb, OverflowCallback ofCb)
  : head(0),
    tail(0),
    count(0),
    packetCallback(pktCb),
    overflowCallback(ofCb) {
}

void ArtNetSerialReceiver::update(Stream& serial, uint16_t maxBytes) {
  uint16_t processed = 0;

  while (processed < maxBytes && serial.available() > 0) {
    int value = serial.read();

    if (value < 0) {
      break;
    }

    push(static_cast<uint8_t>(value));
    processed++;
  }

  parse(1);
}

bool ArtNetSerialReceiver::push(uint8_t b, bool notifyOverflow) {
  bool storedWithoutOverflow = true;

  if (count >= BUFFER_SIZE) {
    tail = (tail + 1) % BUFFER_SIZE;
    count--;
    storedWithoutOverflow = false;

    dropped++;

    if (notifyOverflow && overflowCallback) {
      overflowCallback(dropped);
    }
  }

  ring[head] = b;
  head = (head + 1) % BUFFER_SIZE;
  count++;
  return storedWithoutOverflow;
}

bool ArtNetSerialReceiver::pushFromISR(uint8_t b) {
  return push(b, false);
}

uint8_t ArtNetSerialReceiver::peek(uint16_t index) const {
  return ring[(tail + index) % BUFFER_SIZE];
}

void ArtNetSerialReceiver::copy(uint8_t* dst, uint16_t len) const {
    if (len == 0 || count == 0) {
        return;
    }

    // nicht mehr kopieren als vorhanden
    if (len > count) {
        len = count;
    }

    // erster zusammenhängender Block
    uint16_t firstChunk = min(len, (uint16_t)(BUFFER_SIZE - tail));

    memcpy(dst, &ring[tail], firstChunk);

    // zweiter Block (Wrap-Around)
    if (len > firstChunk) {
        memcpy(dst + firstChunk, &ring[0], len - firstChunk);
    }
}

void ArtNetSerialReceiver::drop(uint16_t bytes) {
  if (bytes > count) {
    bytes = count;
  }

  tail = (tail + bytes) % BUFFER_SIZE;
  count -= bytes;
}

void ArtNetSerialReceiver::reset() {
  head = 0;
  tail = 0;
  count = 0;
  dropped = 0;
}

bool ArtNetSerialReceiver::headerMatches() const {
  if (count < ARTNET_HEADER_SIZE) {
    return false;
  }

  for (uint8_t i = 0; i < ARTNET_HEADER_SIZE; i++) {
    if (peek(i) != ARTNET_ID[i]) {
      return false;
    }
  }

  return true;
}

void ArtNetSerialReceiver::parse(uint8_t maxPackets) {
  uint8_t parsedPackets = 0;

  while (count >= ARTNET_HEADER_SIZE && parsedPackets < maxPackets) {

    if (!headerMatches()) {
      drop(1);
      continue;
    }

    if (count < 10) {
      return;
    }

    // Art-Net OpCode ist Little Endian im Stream
    uint16_t opcode = static_cast<uint16_t>(peek(8)) | (static_cast<uint16_t>(peek(9)) << 8);
    uint16_t packetSize = 0;

    switch (opcode) {
      case 0x5000:  // ArtDMX
      case 0x5100:
        {  // ArtNzs
          if (count < 18) return;

          uint16_t length =
            (static_cast<uint16_t>(peek(16)) << 8) | static_cast<uint16_t>(peek(17));

          if (length < 2 || length > 512 || (length % 2) != 0) {
            drop(1);
            continue;
          }

          packetSize = 18 + length;
          break;
        }

      case 0x2400:
        {  // ArtCommand
          if (count < 14) return;

          uint16_t length =
            (static_cast<uint16_t>(peek(12)) << 8) | static_cast<uint16_t>(peek(13));

          packetSize = 14 + length;
          break;
        }
      case 0x2300:
        {  // ArtDiagData
          if (count < 16) return;

          uint16_t length =
            (static_cast<uint16_t>(peek(14)) << 8) | static_cast<uint16_t>(peek(15));

          packetSize = 16 + length;
          break;
        }

      case 0x8100:
        {  // ArtTodData
          if (count < 24) return;

          uint16_t uidCount = peek(23);
          packetSize = 24 + uidCount * 6;
          break;
        }

      case 0x8300:  // ArtRdm
      case 0x8400:
        {  // ArtRdmSub

          if (count < 24) return;

          uint16_t rdmLength = peek(23);
          packetSize = 24 + rdmLength;
          break;
        }


      case 0x2000: packetSize = 14; break;    // ArtPoll
      case 0x2100: packetSize = 239; break;   // ArtPollReply
      case 0x5200: packetSize = 14; break;    // ArtSync
      case 0x6000: packetSize = 107; break;   // ArtAddress
      case 0x7000: packetSize = 14; break;    // ArtInput
      case 0x8000: packetSize = 14; break;    // ArtTodRequest
      case 0x8200: packetSize = 14; break;    // ArtTodControl
      case 0x9700: packetSize = 19; break;    // ArtTimeCode
      case 0x9800: packetSize = 14; break;    // ArtTimeSync
      case 0x9900: packetSize = 18; break;    // ArtTrigger
      case 0xF200: packetSize = 1038; break;  // ArtFirmwareMaster
      case 0xF300: packetSize = 14; break;    // ArtFirmwareReply
      case 0xF800: packetSize = 34; break;    // ArtIpProg
      case 0xF900: packetSize = 34; break;    // ArtIpProgReply


      default: {
          // unbekanntes Paket: bis zum nächsten Art-Net Header sammeln
          for (uint16_t i = 1; i + ARTNET_HEADER_SIZE <= count; i++) {
            bool found = true;

            for (uint8_t h = 0; h < ARTNET_HEADER_SIZE; h++) {
              if (peek(i + h) != ARTNET_ID[h]) {
                found = false;
                break;
              }
            }

            if (found) {
              packetSize = i;
              break;
            }
          }

          if (packetSize == 0) {
            return;
          }

          break;
        }
    }

    if (packetSize > BUFFER_SIZE) {
      drop(1);
      continue;
    }

    if (count < packetSize) {
      return;
    }

    static uint8_t packet[BUFFER_SIZE];

    copy(packet, packetSize);
    drop(packetSize);

    if (dropped > 0) {
      dropped = 0;
    }

    if (packetCallback != nullptr) {
      packetCallback(packet, packetSize);
    }

    parsedPackets++;
  }
}
