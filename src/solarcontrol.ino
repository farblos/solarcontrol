//
// solarcontrol.ino - new life for an old solar hot water system.
//

#include <DallasTemperature.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <RTClib.h>
#include <SdFat.h>
#include <Wire.h>
#include <avr/wdt.h>

#ifdef DEBUG
#define debugln( message )       Serial.println( message )
#define debugnm( message, base ) Serial.println( message, base )
#else
#define debugln( message )
#define debugnm( message, base )
#endif

//{{{ miscellaneous constants

// main loop delay in ms per cycle.  This should be small enough
// to not trigger the watchdog timer in regular operation.
#define MAIN_DELAY              1000

// debounce delay in ms
#define DEBOUNCE_DELAY          200

// light levels to accumulate before starting pump
#define INIT_LIGHT_LEVEL        3000

// number of cycles to wait before checking supply flow
// temperature when pumping
#define INIT_PUMP_CYCLES        120

// minimum temperature difference between supply flow temperature
// and tank temperature in centidegrees Celsius to keep the pump
// working
#define TEMP_DELTA              500

//}}}

//{{{ physical pin-out

// in addition to the pins mentioned below use:
//
// - A4, A5  for LCD and RTC access on I2C
//
// - 10 - 13 for SD card access on SPI
//
// The control button must be on a pin that can trigger
// interrupts.
#define INPUT_LIGHT_SENSOR      A0
#define INPUT_CONTROL_BUTTON    2
#define INOUT_ONE_WIRE          6
#define OUTPUT_PUMP_RELAIS_N    7
#define OUTPUT_PUMP_RELAIS_L    8
#define OUTPUT_AUX_RELAIS       9

//}}}

//{{{ subsystem and error constants and variables

#define SUBSYS_SENSORS          0x0001
#define SUBSYS_LCD              0x0002
#define SUBSYS_RTC              0x0004
#define SUBSYS_SD               0x0008

// critical subsystems, the failure of which results in the error
// state
#define SUBSYS_CRITICAL         SUBSYS_SENSORS

#define SUBSYS_MASK             0x000f

#define SDLOC_WRITE             0x0010
#define SDLOC_CLOSE             0x0020
#define SDLOC_OPEN              0x0030
#define SDLOC_SYNC              0x0040
#define SDLOC_INIT_BEGIN        0x0050
#define SDLOC_INIT_CHDIR        0x0060
#define SDLOC_INIT_OPEN_READ    0x0070
#define SDLOC_INIT_STAT         0x0080
#define SDLOC_INIT_CLOSE        0x0090
#define SDLOC_INIT_OPEN_WRITE   0x00a0

#define SDLOC_MASK              0x0ff0

#define okp( subsys )  (! (fldss & subsys))

uint8_t fldss = 0;

#define MSGID_ERROR             0x0e
#define MSGID_SD_ERROR          0x0f

#define MSGID_MASK              0x0f

#define MESSAGE_MASK            0x0fff

// message ring size.  Should be larger than one for the message
// ring to be useful.
#define MESSAGE_RING_SIZE       4

// message ring
uint16_t mring[MESSAGE_RING_SIZE] = { 0 };

// message ring write pointer.  Always smaller than
// MESSAGE_RING_SIZE.
byte mrwrp = 0;

// message ring maximum pointer.  Always smaller than or equal to
// MESSAGE_RING_SIZE.  Ahead by one compared to the write pointer
// as long as it does not equal MESSAGE_RING_SIZE.
byte mrmxp = 1;

// message ring read pointer.  Always smaller than the message
// ring maximum pointer.
byte mrrdp = 0;

// report an error of the specified subsystem
#define error( subsys )                                                 \
  error0( MSGID_ERROR, subsys );

// report an error of the SD card subsystem
#define sderr( sdloc ) {                                                \
  error0( MSGID_ERROR,    SUBSYS_SD | sdloc );                          \
  error0( MSGID_SD_ERROR, lfidx << 8 | sd.card()->errorCode() );        \
}

void error0( uint8_t msgid, uint16_t msgvl )
{
  debugnm( (uint16_t)((msgid & MSGID_MASK) << 12) | (msgvl & MESSAGE_MASK), HEX );

  // fail the corresponding subsystem if this is a regular error
  if ( (msgid == MSGID_ERROR) )
    fldss |= (msgvl & SUBSYS_MASK);

  // store message ID and value in message ring
  mring[mrwrp++] = ((msgid & MSGID_MASK) << 12) | (msgvl & MESSAGE_MASK);

  // wrap over pointer, if needed
  if ( mrwrp == MESSAGE_RING_SIZE ) mrwrp = 0;

  // zero the following element in the message ring as wrap-over
  // marker
  mring[mrwrp] = 0;

  if ( mrmxp < MESSAGE_RING_SIZE )
    mrmxp++;
}

uint32_t crmsg()
{
  uint32_t msg = mring[mrrdp++];
  if ( mrrdp == mrmxp ) mrrdp = 0;
  return msg;
}

//}}}

//{{{ control button items

volatile bool cbtnp = false;

unsigned long lits = 0;                         // last-interrupt-timestamp

void ocbtnp()                                   // on-control-button-pressed
{
  // ignore interrupts coming too fast to be real button presses
  unsigned long its = millis();
  if ( its - lits > DEBOUNCE_DELAY ) {
    cbtnp = true;
    lits  = its;
  }
}

//}}}

//{{{ state items

#define STATE_STARTING          '^'
#define STATE_ERROR             '!'
#define STATE_FORCE_ON          '+'
#define STATE_FORCE_OFF         '-'
#define STATE_PUMPING           '@'
#define STATE_WAITING           '*'

char state;
int  ltlvl;
int  pumpc;

// sets the current processing state to the specified new state.
// Initializes variables related to the new state.  The new state
// must be different from the current state.
void setstate( char newstate )
{
  state = newstate;

  switch ( state ) {
  case STATE_PUMPING:
    pumpc = INIT_PUMP_CYCLES;
    break;

  case STATE_WAITING:
    ltlvl = INIT_LIGHT_LEVEL;
    break;
  }
}

//}}}

//{{{ sensor items

// supply flow, return flow, tank sensors
const DeviceAddress SENSOR_SUPPLY = { 0x28, 0xaa, 0xac, 0xdb, 0x3c, 0x14, 0x01, 0xe6 };
const DeviceAddress SENSOR_RETURN = { 0x28, 0xaa, 0xe9, 0x12, 0x3d, 0x14, 0x01, 0xdc };
const DeviceAddress SENSOR_TANK   = { 0x28, 0xaa, 0x33, 0x11, 0x3d, 0x14, 0x01, 0xb7 };

OneWire onewire( INOUT_ONE_WIRE );

DallasTemperature sensors( &onewire );

// returns temperature reading of the specified sensor
int getTemp( const DeviceAddress sensor )
{
  int raw = sensors.getTemp( sensor );
  if ( raw > DEVICE_DISCONNECTED_RAW ) {
    return (float)raw * 0.78125;
  }
  else {
    // report an error only if that has not be done before
    if ( okp( SUBSYS_SENSORS ) )
      error( SUBSYS_SENSORS );
    return -1000;
  }
}

//}}}

//{{{ LCD items

#define LCD_I2C_ADDRESS         0x27

#define LCD_CHAR_A_UMLAUT       "\xe1"
#define LCD_CHAR_O_UMLAUT       "\xef"
#define LCD_CHAR_U_UMLAUT       "\xf5"
#define LCD_CHAR_S_SHARP        "\xe2"
#define LCD_CHAR_DEGREE         "\xdf"

LiquidCrystal_I2C lcd( LCD_I2C_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE );

//}}}

//{{{ RTC items

RTC_DS1307 rtc;

//}}}

//{{{ SD card items

#define LOG_FILE_MAXSIZE        10485760
#define LOG_FILE_SYNC_DELTA     1024

// log directory name.  Create log directory structure as
// follows:
//
//   rm -rf /media/usd00/solarlog
//   mkdir  /media/usd00/solarlog
//   for i in 9 8 7 6 5 4 3 2 1 0; do
//     test $i == 0 && sleep 2
//     touch /media/usd00/solarlog/log$i.csv
//   done
const char LOG_DIR_NAME[] = "/solarlog";

// log file name, which is calculated from the log file template
// and the specified log file index
#define lfn( lfidx ) (LOG_FILE_TEMPLATE[3] = '0' + lfidx, LOG_FILE_TEMPLATE)
char LOG_FILE_TEMPLATE[] = "log0.csv";

SdFat    sd;
SdFile   lfile;
byte     lfidx;
uint32_t lfssz;

void rtc2sd( uint16_t* date, uint16_t* time )
{
  DateTime now = rtc.now();
  *date = FAT_DATE( now.year(), now.month(),  now.day() );
  *time = FAT_TIME( now.hour(), now.minute(), now.second() );
}

//}}}

//{{{ updtlog

// write the specified sensor data and the current state to the
// current log file.  Rotate or sync log files as needed.
void updtlog( int temps, int tempr, int tempt, int light )
{
  if ( (! okp( SUBSYS_SD )) )
    return;

  lfile.print( rtc.now().unixtime() );
  lfile.print( "," );
  lfile.print( state );
  lfile.print( "," );
  lfile.print( temps );
  lfile.print( "," );
  lfile.print( tempr );
  lfile.print( "," );
  lfile.print( tempt );
  lfile.print( "," );
  lfile.print( light );
  lfile.print( "," );
  lfile.print( (state == STATE_PUMPING) ? pumpc : 0 );
  lfile.print( "," );
  lfile.print( (state == STATE_WAITING) ? ltlvl : 0 );
  lfile.print( "," );
  lfile.print( fldss );
  lfile.println();

  if ( lfile.getWriteError() ) {
    lfile.clearWriteError();
    sderr( SDLOC_WRITE );
    debugln( "Cannot write to log file." );
  }

  uint32_t lfsz = lfile.fileSize();
  if ( lfsz > LOG_FILE_MAXSIZE ) {
    // switch to next log file
    if ( (! lfile.close() ) ) {
      sderr( SDLOC_CLOSE );
      debugln( "Cannot close log file after writing." );
    }
    if ( lfidx == 9 )
      lfidx = 0;
    else
      lfidx++;
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.open( lfn( lfidx ), O_WRITE | O_TRUNC | O_APPEND ) ) ) {
      sderr( SDLOC_OPEN );
      debugln( "Cannot open next log file for writing." );
    }
    lfssz = 0;
  }
  else if ( (lfsz - lfssz) > LOG_FILE_SYNC_DELTA ) {
    // sync log file if unsynched delta gets too large
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.sync() ) ) {
      sderr( SDLOC_SYNC );
      debugln( "Cannot sync log file." );
    }
    lfssz = lfsz;
  }

  // try to close the log file as a last resort in case of errors
  if ( (! okp( SUBSYS_SD )) ) {
    lfile.close();
  }
}

//}}}

//{{{ updtlcd

void lcddec( char row[], byte pos, char ind, int val )
{
  row[pos++] = ind;
  if ( val < -999 ) {
    row[pos++] = '-';
    row[pos++] = '-';
    row[pos++] = '-';
    row[pos++] = '-';
  }
  else if ( val < 0 ) {
    row[pos++] = '-';                val = -val;
    row[pos++] = '0' + (val /  100); val = val %  100;
    row[pos++] = '0' + (val /   10); val = val %   10;
    row[pos++] = '0' + (val);
  }
  else if ( 9999 < val ) {
    row[pos++] = '+';
    row[pos++] = '+';
    row[pos++] = '+';
    row[pos++] = '+';
  }
  else {
    row[pos++] = '0' + (val / 1000); val = val % 1000;
    row[pos++] = '0' + (val /  100); val = val %  100;
    row[pos++] = '0' + (val /   10); val = val %   10;
    row[pos++] = '0' + (val);
  }
}

void lcdhex( char row[], byte pos, char ind, uint16_t val )
{
  uint8_t digit;
  row[pos++] = ind;
  digit = val >> 12; val &= 0x0fff; row[pos++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
  digit = val >>  8; val &= 0x00ff; row[pos++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
  digit = val >>  4; val &= 0x000f; row[pos++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
  digit = val >>  0;                row[pos++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
}

// (incomplete)
void updtlcd( int temps, int tempr, int tempt, int light )
{
  if ( (! okp( SUBSYS_LCD )) )
    return;

  char row[17];
  row[16] = '\0';

  // format: "sNNNNrNNNNcNNNNs"
  lcddec( row,  0, 's', temps );
  lcddec( row,  5, 'r', tempr );
  if ( (state == STATE_PUMPING) )
  lcddec( row, 10, 'c', pumpc );
  else if ( (state == STATE_WAITING) )
  lcddec( row, 10, 'c', ltlvl );
  else
  lcddec( row, 10, 'c', 0 );
          row[ 15] = state;
  lcd.setCursor( 0, 0 );
  lcd.print( row );

  uint16_t msg = crmsg();

  // format: "tNNNNlNNNNeNNNN "
  lcddec( row,  0, 't', tempt );
  lcddec( row,  5, 'l', light );
  if ( (msg == 0) ) {
          row[ 10] = ' ';
          row[ 11] = ' ';
          row[ 12] = ' ';
          row[ 13] = ' ';
          row[ 14] = ' ';
  lcd.noBacklight();
  }
  else {
  lcdhex( row, 10, 'e', msg );
  lcd.backlight();
  }
          row[ 15] = ' ';
  lcd.setCursor( 0, 1 );
  lcd.print( row );
}

//}}}

//{{{ setup

void setup()
{
#ifdef DEBUG
  // initialize serial line and wait for input on it
  Serial.begin( 9600 );
  while ( ! Serial.available() ) {
  }
#endif

  // initialize state
  state = STATE_STARTING;

  // initialize temperature sensors
  sensors.begin();
  if ( (sensors.isConnected( SENSOR_SUPPLY )) &&
       (sensors.isConnected( SENSOR_RETURN )) &&
       (sensors.isConnected( SENSOR_TANK   )) ) {
    sensors.setResolution( 12 );
  }
  else {
    error( SUBSYS_SENSORS );
    debugln( "Cannot initialize sensor." );
  }

  // initialize LCD
  Wire.begin();
  Wire.beginTransmission( LCD_I2C_ADDRESS );
  if ( Wire.endTransmission() == 0 ) {
    lcd.begin( 16, 2 );
    lcd.noBacklight();
  }
  else {
    error( SUBSYS_LCD );
    debugln( "Cannot initialize LCD." );
  }

  // initialize RTC
  if ( (rtc.begin()) ) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  else {
    error( SUBSYS_RTC );
    debugln( "Cannot initialize RTC." );
  }

  // initialize SD card and change to log directory
  SdFile::dateTimeCallback( rtc2sd );
  if ( (okp( SUBSYS_SD )) &&
       (! sd.begin( SS, SPI_HALF_SPEED )) ) {
    sderr( SDLOC_INIT_BEGIN );
    debugln( "Cannot initialize SD." );
  }
  if ( (okp( SUBSYS_SD )) &&
       (! sd.chdir( LOG_DIR_NAME )) ) {
    sderr( SDLOC_INIT_CHDIR );
    debugln( "Cannot change to log directory." );
  }

  // ensure all log files exist and determine most recent one
  uint16_t mrpdate = 0;
  uint16_t mrptime = 0;
  byte     mrlfidx = 0;
  for ( byte i = 0; i < 10; i++ ) {
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.open( lfn( i ), O_READ )) ) {
      sderr( SDLOC_INIT_OPEN_READ );
      debugln( "Cannot open log file for reading." );
    }
    uint16_t pdate = 0;
    uint16_t ptime = 0;
    if ( (okp( SUBSYS_SD )) &&
         (! lfile.getModifyDateTime( &pdate, &ptime )) ) {
      sderr( SDLOC_INIT_STAT );
      debugln( "Cannot determine modification timestamp." );
    }
    if ( (! lfile.close() ) ) {
      sderr( SDLOC_INIT_CLOSE );
      debugln( "Cannot close log file after reading." );
    }
    if ( (pdate > mrpdate) ||
         ((pdate == mrpdate) && (ptime > mrptime)) ) {
      mrpdate = pdate;
      mrptime = ptime;
      mrlfidx = i;
    }
  }

  // open newest log file for appending
  lfidx = mrlfidx;
  if ( (okp( SUBSYS_SD )) &&
       (! lfile.open( lfn( lfidx ), O_WRITE | O_APPEND )) ) {
    error( SDLOC_INIT_OPEN_WRITE );
    debugln( "Cannot open initial log file for writing." );
  }
  if ( (okp( SUBSYS_SD )) ) {
    lfssz = lfile.fileSize();
  }

  // activate internal pull-up on control button
  pinMode( INPUT_CONTROL_BUTTON, INPUT_PULLUP );

  // initialize relais MOS-FETs
  pinMode( OUTPUT_PUMP_RELAIS_N, OUTPUT );
  digitalWrite( OUTPUT_PUMP_RELAIS_N, LOW );
  pinMode( OUTPUT_PUMP_RELAIS_L, OUTPUT );
  digitalWrite( OUTPUT_PUMP_RELAIS_L, LOW );
  pinMode( OUTPUT_AUX_RELAIS, OUTPUT );
  digitalWrite( OUTPUT_AUX_RELAIS, LOW );

  // enable watchdog
  wdt_enable( WDTO_8S );
}

//}}}

//{{{ loop

void loop()
{
  sensors.requestTemperatures();
  int temps = getTemp( SENSOR_SUPPLY );
  int tempr = getTemp( SENSOR_RETURN );
  int tempt = getTemp( SENSOR_TANK );

  int light = analogRead( INPUT_LIGHT_SENSOR );

  // determine new state
  switch ( state ) {
  case STATE_STARTING:
    if ( (! okp( SUBSYS_CRITICAL )) ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      // should not happen
      cbtnp = false;
    }
    else if ( (temps != tempr) || (tempr != tempt) ) {
      attachInterrupt( digitalPinToInterrupt( INPUT_CONTROL_BUTTON ),
                       ocbtnp, FALLING );
      updtlog( 0, 0, 0, 0 );
      setstate( STATE_PUMPING );
    }
    break;

  case STATE_ERROR:
    break;

  case STATE_FORCE_ON:
    if ( (! okp( SUBSYS_CRITICAL )) ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_WAITING );
    }
    break;

  case STATE_FORCE_OFF:
    if ( (! okp( SUBSYS_CRITICAL )) ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_PUMPING );
    }
    break;

  case STATE_WAITING:
    if ( (! okp( SUBSYS_CRITICAL )) ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_FORCE_OFF );
    }
    else if ( ltlvl > 0 ) {
      if ( false )
        ; // alignment no-op
      else if ( (okp( SUBSYS_SENSORS )) &&
                (tempt >= 100) )
        ltlvl -= light / (tempt / 100);
      else if ( (okp( SUBSYS_SENSORS )) )
        ltlvl -= light;
      else
        ltlvl -= light / 64;
      if ( ltlvl < 0 ) ltlvl = 0;
    }
    else {
      setstate( STATE_PUMPING );
    }
    break;

  case STATE_PUMPING:
    if ( (! okp( SUBSYS_CRITICAL )) ) {
      setstate( STATE_ERROR );
    }
    else if ( cbtnp ) {
      cbtnp = false;
      setstate( STATE_FORCE_ON );
    }
    else if ( pumpc > 0 ) {
      pumpc--;
    }
    else if ( temps - tempt < TEMP_DELTA ) {
      setstate( STATE_WAITING );
    }
    break;
  }

  // update display right away after start-up
  updtlcd( temps, tempr, tempt, light );

  // update log file only after start-up
  if ( (state != STATE_STARTING) ) {
    updtlog( temps, tempr, tempt, light );
  }

  // operate pump depending on state
  if ( (state == STATE_FORCE_ON) || (state == STATE_PUMPING) ) {
    digitalWrite( OUTPUT_AUX_RELAIS, HIGH );
  }
  else {
    digitalWrite( OUTPUT_AUX_RELAIS, LOW );
  }

  wdt_reset();

  delay( MAIN_DELAY );
}

//}}}

// Local Variables:
// mode: c++
// End:
