#!./.venv/bin/python

import json
from digi.xbee.models.address import XBee16BitAddress, XBee64BitAddress
from digi.xbee.models.options import TransmitOptions
import serial
from digi.xbee.packets.common import TransmitPacket
from digi.xbee.devices import DigiMeshDevice, RemoteDigiMeshDevice

DUMMY_ADDR = b"\x01\x02\x03\x04\x05\x06\x07\x08"
PAYLOAD = {'cmd': 0x1, 'value': ''}

if __name__ == '__main__':
    dummy_xbee = DigiMeshDevice('/dev/ttyUSB0', 9600)
    addr = XBee64BitAddress(DUMMY_ADDR)
    remote = RemoteDigiMeshDevice(dummy_xbee, addr, node_id="DUMMY")
    tx_options = TransmitOptions.NONE.value
    packet = TransmitPacket(dummy_xbee._get_next_frame_id(),
                            addr,
                            XBee16BitAddress.UNKNOWN_ADDRESS,
                            0,
                            tx_options,
                            rf_data=b"HELLO, WORLD")
    print(packet.to_dict())
