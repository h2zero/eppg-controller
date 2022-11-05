Import("env")
imgs = env["FLASH_EXTRA_IMAGES"]
imgs[0] = ('0x0000', 'uf2/esp32s3/bootloader.bin')

upl = env["UPLOADERFLAGS"]
for i, s in enumerate(upl):
    if s.endswith("bootloader.bin"):
        upl[i] = 'uf2/esp32s3/bootloader.bin'
        break
