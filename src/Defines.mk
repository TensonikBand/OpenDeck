#common defines
DEFINES := \
-DNDEBUG \
-DARCH=ARCH_AVR8 \
-DF_CPU=16000000UL \
-DF_USB=16000000UL \
-DBOARD=BOARD_NONE \
-DUSE_STATIC_OPTIONS=0 \
-DUSB_DEVICE_ONLY \
-DFIXED_CONTROL_ENDPOINT_SIZE=8 \
-DFIXED_NUM_CONFIGURATIONS=1 \
-DHID_VENDOR_ID=0x1209 \
-DHID_PRODUCT_ID=0x8473 \
-DMIDI_VENDOR_ID=0x1209 \
-DMIDI_PRODUCT_ID=0x8472 \
-DSTRING_BUFFER_SIZE=1 \
-DDBMS_MAX_SECTIONS=7 \
-DDBMS_MAX_BLOCKS=7 \
-DSYSEX_MAX_BLOCKS=7 \
-DSYSEX_MAX_SECTIONS=10 \
-DMIDI_BAUD_RATE=31250

ifeq ($(findstring opendeck,$(MAKECMDGOALS)), opendeck)
MCU := atmega32u4
MCU_avrdude := m32u4
DEFINES += -DBOARD_OPEN_DECK
DEFINES += -DEEPROM_SIZE=1024
BOOT_START_ADDR := 0x7000
FLASH_SIZE_START_ADDR := 0xAC
FLASH_SIZE_END_ADDR := 0xB0
endif

ifeq ($(findstring leonardo,$(MAKECMDGOALS)), leonardo)
MCU := atmega32u4
MCU_avrdude := m32u4
DEFINES += -DBOARD_A_LEO
DEFINES += -DEEPROM_SIZE=1024
BOOT_START_ADDR := 0x7000
FLASH_SIZE_START_ADDR := 0xAC
FLASH_SIZE_END_ADDR := 0xB0
endif

ifeq ($(findstring pro_micro,$(MAKECMDGOALS)), pro_micro)
MCU := atmega32u4
MCU_avrdude := m32u4
DEFINES += -DBOARD_A_PRO_MICRO
DEFINES += -DEEPROM_SIZE=1024
BOOT_START_ADDR := 0x7000
FLASH_SIZE_START_ADDR := 0xAC
FLASH_SIZE_END_ADDR := 0xB0
endif

ifeq ($(findstring mega,$(MAKECMDGOALS)), mega)
MCU := atmega2560
MCU_avrdude := m2560
DEFINES += -DBOARD_A_MEGA
DEFINES += -DEEPROM_SIZE=4096
endif

ifeq ($(findstring uno,$(MAKECMDGOALS)), uno)
MCU := atmega328p
MCU_avrdude := m328p
DEFINES += -DBOARD_A_UNO
DEFINES += -DEEPROM_SIZE=1024
endif

ifeq ($(findstring 16u2,$(MAKECMDGOALS)), 16u2)
MCU := atmega16u2
MCU_avrdude := m16u2
DEFINES += -DBOARD_A_16u2
DEFINES += -DEEPROM_SIZE=512
BOOT_START_ADDR := 0x3000
endif

DEFINES += -DBOOT_START_ADDR=$(BOOT_START_ADDR)

ifeq ($(findstring boot,$(MAKECMDGOALS)), boot)
DEFINES += \
-DORDERED_EP_CONFIG \
-DNO_SOF_EVENTS \
-DUSE_RAM_DESCRIPTORS \
-DNO_INTERNAL_SERIAL \
-DDEVICE_STATE_AS_GPIOR \
-DNO_DEVICE_REMOTE_WAKEUP \
-DNO_DEVICE_SELF_POWER
endif

ifeq ($(findstring fw,$(MAKECMDGOALS)), fw)
DEFINES += \
-DUSE_FLASH_DESCRIPTORS \
-DINTERRUPT_CONTROL_ENDPOINT \
-DMIDI_SYSEX_ARRAY_SIZE=45 \
-DRING_BUFFER_SIZE=50
endif

ifeq ($(MCU),atmega32u4)
FUSE_UNLOCK := 0x3f
FUSE_EXT := 0xc8
FUSE_HIGH := 0xd0
FUSE_LOW := 0xff
FUSE_LOCK := 0x2f
endif

ifeq ($(MCU),atmega16u2)
FUSE_UNLOCK := 0x3f
FUSE_EXT := 0xf0
FUSE_HIGH := 0xd3
FUSE_LOW := 0xff
FUSE_LOCK := 0x2f
endif

ifeq ($(MCU),atmega2560)
FUSE_UNLOCK := 0x3f
FUSE_EXT := 0xfc
FUSE_HIGH := 0xd6
FUSE_LOW := 0xff
FUSE_LOCK := 0x2f
endif

ifeq ($(MCU),atmega328p)
FUSE_UNLOCK := 0x3f
FUSE_EXT := 0x04
FUSE_HIGH := 0xd6
FUSE_LOW := 0xff
FUSE_LOCK := 0x2f
endif