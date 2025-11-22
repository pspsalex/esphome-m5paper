# M5Paper components for esphome

This repository contains custom components to get esphome up and running on an M5Paper with LVGL.
The code is tested and works on an M5Paper v1.0, but should work on a V1.1 as well.

A sample configuration file is available in [m5paper.yaml](m5paper.yaml).

You can use either the esp-idf or the Arduino platform.

If you plan on running the app in eqmu, please use esp-idf.

## Emulating with QEMU

There are a few prerequisites needed before the generated app can run in esp-qemu:

- A version of esp-qemu which simulates the IT8951E. See `https://github.com/pspsalex/qemu`
- A binary dump of the e-fuses, either dumped from an ESP32, or from the below file
- A compiled binary (not ELF) image.
- A flash image 16MB in size
- Bridged network configuration so esp-qemu can contact a HA instance on your LAN

### Obtaining esp-qemu

See [README.m5paper.md](https://github.com/pspsalex/qemu/blob/m5paper/README.m5paper.md)

### E-fuses

Easiest is to generate the e-fuses file using the command below:

```bash
cat <<EOL | xxd -r - esp32-efuse.bin
00000000: 0000 0000 586e 5a7e b994 e200 00a2 0000  ....XnZ~........
00000010: 3303 0000 0000 1000 0400 0000 0000 0000  3...............
00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000040: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000060: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000070: 0000 0000 0000 0000 0000 0000            ............
EOL
```

### Compile the app

```bash
esphome compile m5paper.yaml
```

When compilation is finished, a binary file will be available in `.esphome/build/m5paper/.pioenvs/m5paper/firmware.factory.bin`.

Note: replace "m5paper" with the name of your project

### Adjust the image size

The simples way to get a 16MB image out of the compiled binary is to truncate the file to 16MB size:
```bash
truncate -s 16777216 .esphome/build/m5paper/.pioenvs/m5paper/firmware.factory.bin
```

### Configure the network

If you want to have networking access, bridge the TAP interface to your main network.

See `https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/esp32/tap-networking.md` for more info.

```bash
# If /dev/net does not exist, first create it:
mkdir /dev/net
mknod /dev/net/tun c 10 200
chmod 666 /dev/net/tun

# Configure the network:
ip link add br0 type bridge
ip tuntap add dev tap0 mode tap
ip link set tap0 master br0
ip link set eth0 master br0
ip link set tap0 up
ip link set br0 up
```


### Running the app

Only add the "`-nic tap,model...`" argument if you need networking.

```bash
qemu-system-xtensa -m 4M -machine m5paper,gt911_address=0x5d \
    -drive file=.esphome/build/m5paper/.pioenvs/m5paper/firmware.factory.bin,if=mtd,format=raw \
    -global driver=timer.esp32.timg,property=wdt_disable,value=true \
    -serial mon:stdio \
    -drive id=efuse,if=none,format=raw,file=esp32-efuse.bin \
    -global driver=nvram.esp32.efuse,property=drive,value=efuse \
    -global driver=it8951e,property=rotation,value=0 \
    -nic tap,model=open_eth,ifname=tap0,downscript=no,script=no
```

## Acknowledgements

The code is based on mulitple sources:

- https://github.com/jesserockz/m5paper-esphome
  - https://github.com/marcinkowalczyk/m5paper_esphome
    - https://github.com/sebirdman/m5paper_esphome
- https://github.com/m5stack/M5EPD

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.
