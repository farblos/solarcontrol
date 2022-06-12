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

# upload counter magic.  Ensure every upload is done with a
# unique upload number, which will be written to the EEPROM by
# the sketch.
UPLOAD_COUNTER_FILE :=	upload.cnt
UPLOAD_COUNTER :=	$(shell cat $(UPLOAD_COUNTER_FILE) 2>/dev/null || echo 0)

CPPFLAGS +=		-DUPLOAD_COUNTER=$(UPLOAD_COUNTER)

# enable SD card CRC checking for SPI
CPPFLAGS +=		-DUSE_SD_CRC=2

# provide a hook for specifying defines on the commandline
CPPFLAGS +=		$(USER_DEFINES)

# let macros "__DATE__" and "__TIME__" expand to UTC
export TZ :=		UTC

include $(ARDUINO_DIR)/Arduino.mk

# extend target "upload" by a test whether the internal upload
# counter variable is set.  This effectively blocks regular "make
# upload" from Arduino.mk.
upload:			test_upload_counter

.PHONY:			test_upload_counter
test_upload_counter:
			@if test -z "$(UPLOAD_COUNTER_0)"; then			\
			  echo "Cannot upload without upload counter." 1>&2;	\
			  echo "Use target \"cupload\" instead." 1>&2;		\
			  exit 1;						\
			fi

# make a "counted upload", which is a regular upload with the
# upload counter being incremented after successfull upload
.PHONY:			cupload
cupload:
			$(MAKE) UPLOAD_COUNTER_0=$(UPLOAD_COUNTER) upload
			@echo "$$(( $(UPLOAD_COUNTER) + 1 ))" > $(UPLOAD_COUNTER_FILE)
