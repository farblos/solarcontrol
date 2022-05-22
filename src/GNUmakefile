#
# GNUmakefile - arduino project makefile.
#

# system configuration.  Arduino.mk does not seem to get the
# Arduino version right.
ARDUINO_DIR :=		/usr/share/arduino
ARDUINO_VERSION :=	$(shell cat $(ARDUINO_DIR)/lib/version.txt | \
			        sed 's/^[0-9]://;s/\.\([0-9]\)\([^0-9]\)/.0\1\2/g;s/\.//g;s/\+.*//')
AVR_TOOLS_PATH :=	/bin
AVRDUDE_CONF :=		/etc/avrdude.conf

# board configuration
BOARD_TAG :=		nano
BOARD_SUB :=		atmega328

# sketch configuration.  Must specify these explicitly since the
# automatic library detection of Arduino.mk does not work
# reliably.
USER_LIB_PATH :=	$(realpath ../../libraries)
ARDUINO_LIBS :=					\
			Adafruit_BusIO		\
			DallasTemperature	\
			NewliquidCrystal	\
			OneWire			\
			RTClib			\
			SPI			\
			SdFat			\
			Wire

# commandline configuration
CPPFLAGS += $(USER_DEFINES)

include $(ARDUINO_DIR)/Arduino.mk
