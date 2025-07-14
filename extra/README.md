## SSD1306 I2C 0.98" Network info
Thanks to [ssd1306_linux](https://github.com/armlabs/ssd1306_linux).

Copy `S99oled_display` to `/etc/init.d/`

```bash
scp S99oled_display root@172.32.0.93/etc/init.d/
```

Copy `ssd1306_bin` to `/root`

```bash
scp ssd1306_bin root@172.32.0.93/root
```

reboot

