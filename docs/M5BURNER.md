# Publishing Stackfall on M5Burner

M5Burner is M5Stack's firmware distribution app. This guide is for uploading the Stackfall image so
others can flash it without building.

## Build the release image

```bash
pio run -e m5stack-sticks3
# image: .pio/build/m5stack-sticks3/firmware.bin
```

The build asserts no Wi-Fi/BLE/OTA symbols link (`tools/check_image.py`, `CHECK_IMAGE,PASS`).

## Upload in the M5Burner desktop app

1. Open **M5Burner** → **User** tab → sign in.
2. **Add / Upload firmware** (custom firmware).
3. Fill in:
   - **Name:** `Stackfall`
   - **Device:** M5StickC / StickS3 (ESP32-S3)
   - **Version:** `1.0.0`
   - **Description:** *Tilt-controlled falling-block game for the M5StickS3. Steer, rotate and drop
     by tilting the stick — the BMI270 accelerometer is the controller. Original game (SRS-style),
     not affiliated with Tetris. Source: github.com/fcavalcantirj/sticks3-tetris*
   - **Cover image:** `docs/img/play1.jpg`
   - **Firmware file:** `.pio/build/m5stack-sticks3/firmware.bin`
   - **Flash address:** `0x10000` (app partition; use the same offset the M5Burner default template
     for the S3 expects).
4. Publish.

## Flash offsets (for reference)

The full flash layout comes from the PlatformIO build (`bootloader.bin @ 0x0`, `partitions.bin @
0x8000`, `boot_app0.bin @ 0xe000`, `firmware.bin @ 0x10000`). M5Burner's S3 template handles the
bootloader/partition parts; you supply the app image at `0x10000`.
