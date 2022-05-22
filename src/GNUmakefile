#
# GNUmakefile - arduino project makefile.
#

# system configuration.  Arduino.mk does not seem to get the
# arduino version right.  See
# https://github.com/sudar/Arduino-Makefile/issues/624.
ARDUINO_DIR :=		/usr/share/arduino
ARDUINO_VERSION :=	$(shell cat $(ARDUINO_DIR)/lib/version.txt | \
			        sed 's/^[0-9]://;s/\.\([0-9]\)\>/.0\1/g;s/\.//g;s/\+.*$$//')
AVR_TOOLS_PATH :=	/bin
AVRDUDE_CONF :=		/etc/avrdude.conf

# board configuration
BOARD_TAG :=		nano
BOARD_SUB :=		atmega328

# sketch configuration.  Must specify the arduino libraries
# explicitly since the automatic library detection of Arduino.mk
# does not work reliably.
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

# provide a hook for specifying defines on the commandline
CPPFLAGS +=		$(USER_DEFINES)

# let macros "__DATE__" and "__TIME__" expand to UTC
export TZ =		UTC

include $(ARDUINO_DIR)/Arduino.mk
