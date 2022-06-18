#
# GNUmakefile - arduino project makefile.
#

# system configuration.  Arduino.mk does not seem to get the
# arduino version right.  See
# https://github.com/sudar/Arduino-Makefile/issues/624.
ARDUINO_DIR :=		/usr/share/arduino
ARDUINO_VERSION :=	$(shell cat $(ARDUINO_DIR)/lib/version.txt | \
			        sed 's/^[0-9]://;s/\.\([0-9]\)\>/.0\1/g;s/\.//g;s/+.*$$//')
AVR_TOOLS_PATH :=	/bin
AVRDUDE_CONF :=		/etc/avrdude.conf

# board configuration
BOARD_TAG :=		nano
BOARD_SUB :=		atmega328

# sketch configuration.  Specify the arduino libraries explicitly
# since the automatic library detection of Arduino.mk does not
# work reliably.
TARGET :=		solarcontrol
USER_LIB_PATH :=	$(realpath ../../libraries)
ARDUINO_LIBS :=					\
			Adafruit_BusIO		\
			DallasTemperature	\
			EEPROM			\
			NewliquidCrystal	\
			OneWire			\
			RTClib			\
			SPI			\
			SdFat			\
			Wire
ARDUINO_QUIET :=	1

# upload counter magic.  Tracks uploads in the upload directory
# and provides a unique upload counter at sketch compile time.
UPLOAD_DIR :=		uploads
UPLOAD_COUNTER :=	$(shell ls -vd "$(UPLOAD_DIR)/$(TARGET)".* 2>/dev/null | \
			        awk -F . '{ uc = $$NF + 1 } END { print uc + 0 }')
CPPFLAGS +=		-DUPLOAD_COUNTER=$(UPLOAD_COUNTER)

# enable SD card CRC checking for SPI
CPPFLAGS +=		-DUSE_SD_CRC=2

# provide a hook for specifying defines on the commandline
CPPFLAGS +=		$(USER_DEFINES)

# let macros "__DATE__" and "__TIME__" expand to UTC
export TZ :=		UTC

include $(ARDUINO_DIR)/Arduino.mk

# extend target "upload" by a test whether the upload is tracked.
# This effectively blocks regular "make upload" from Arduino.mk.
upload:			test_untracked_upload

.PHONY:			test_untracked_upload
test_untracked_upload:
			@if test -z "$(TRACKED_UPLOAD)"; then			\
			  echo -n "Cannot process untracked upload.  " 1>&2;	\
			  echo    "Use target \"tupload\" instead." 1>&2;	\
			  exit 1;						\
			fi

# force a recompile before a tracked upload to get correct date,
# time, and upload counter compiled in
ifdef TRACKED_UPLOAD
$(LOCAL_OBJS):		force_tracked_recompile

.PHONY:			force_tracked_recompile
force_tracked_recompile:
endif

# make a "tracked upload", which is a regular upload with the
# uploaded sketch being saved to the upload directory
.PHONY:			tupload
tupload:
			$(MAKE) TRACKED_UPLOAD=1 upload
			mkdir -p $(UPLOAD_DIR)/$(TARGET).$(UPLOAD_COUNTER)
			cp $(LOCAL_INO_SRCS) $(UPLOAD_DIR)/$(TARGET).$(UPLOAD_COUNTER)
