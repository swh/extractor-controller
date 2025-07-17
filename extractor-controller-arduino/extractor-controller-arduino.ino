#include <limits.h>
#include <sps30.h>
#include <LiquidCrystal_I2C.h>
#include <DHTStable.h>
#include <IRremote.hpp>
#include <RTC.h>
#include <WiFiS3.h>

#include "wifi.h"

#define CHAR_HEIGHT 8

#define PASIRPIN D2
#define INDLEDPIN D3
#define IRLEDPIN D4
#define DHTPIN D7
#define OVERRIDESWPIN D6
#define CLEANSWPIN D5

#define EXT_UP 1
#define EXT_OFF 2

/* How often to fire the data callback */
const unsigned long MILLIS_MINUTE = 60*1000;
unsigned long next_timer = MILLIS_MINUTE;

/* If this is not a power of two it will glitch slightly when millis() rolls */
const unsigned int HISTORY_BUFFER = 16;
const unsigned int HISTORY_MINUTES = 10;
float c1_history[HISTORY_BUFFER];

/* PIR must have been activated in this time for fans to turn on */
const unsigned long PIR_WINDOW = 60 * MILLIS_MINUTE;

DHTStable DHT;

LiquidCrystal_I2C lcd(0x20, 20, 4);

uint64_t rawDataOn[] =  {0xFF20B004CF30FF00, 0x20B004CF30FF00};
uint64_t rawDataOff[] = {0xFF20B0046F90FF00, 0x20B0046F90FF00};

int ext_fan_speed = 0;
float particle_counts[4] = {NAN, NAN, NAN, NAN};
float temperature = NAN;
float relative_humidity = NAN;
volatile int passive_ir = 0;
volatile int override_switch = 0;
volatile int clean_switch = 0;
unsigned long pir_time = 0;

int fan_override = 0;
unsigned long fan_override_start = 0;
unsigned long fan_override_end = 0;

void lcdMessage(char *l1, char *l2=NULL, char *l3=NULL, char *l4=NULL) {
  char *lines[] = {l1, l2, l3, l4};
  int not_null = 0;
  for (int i=0; i<4; i++) {
    if (lines[i]) not_null++;
  }
  int offset = 0;
  if (not_null < 3) {
    offset = 1;
  }
  lcd.clear();
  for (int i=0; i<4; i++) {
    if (lines[i]) {
      lcd.setCursor(0, i+offset);
      lcd.print(lines[i]);
      Serial.print(lines[i]);
      Serial.print(" ");
    }
  }
  Serial.println("");
}

void setup() {
  int16_t ret;
  uint8_t auto_clean_days = 4;
  uint32_t auto_clean;

  pinMode(IRLEDPIN, OUTPUT);
  pinMode(INDLEDPIN, OUTPUT);
  pinMode(PASIRPIN, INPUT);
  pinMode(OVERRIDESWPIN, INPUT_PULLUP);
  pinMode(CLEANSWPIN, INPUT_PULLUP);

  Serial.begin(115200);

  lcd.init();
  delay(100);

  lcd.clear();
  lcd.backlight();
  lcdMessage("Initialising\n");

  wiFiInit();
  delay(1000);

  const byte set_bits[6] = {
   B00000,
   B10000,
   B11000,
   B11100,
   B11110,
   B11111,
  };

  for (int i=0; i<6; i++) {
    byte symbol[CHAR_HEIGHT];
    for (int j=0; j<CHAR_HEIGHT-1; j++) {
      symbol[j] = set_bits[i];
    }
    symbol[CHAR_HEIGHT-1] = set_bits[0];
    lcd.createChar(i, symbol);
  }
  byte symbol[CHAR_HEIGHT] = {
    set_bits[2],
    set_bits[3],
    set_bits[4],
    set_bits[5],
    set_bits[4],
    set_bits[3],
    set_bits[2],
    set_bits[0],
  };
  lcd.createChar(6, symbol);

  lcdMessage("SPS30 initialising");
  sensirion_i2c_init();
  delay(100);

  while (sps30_probe() != 0) {
    lcdMessage("SPS30 sensor", "probing failed");
    delay(500);
  }

  Serial.print("SPS sensor probing successful\n");

  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
  if (ret) {
    Serial.print("error setting the auto-clean interval: ");
    Serial.println(ret);
  }

  ret = sps30_start_measurement();
  if (ret < 0) {
    lcdMessage("SPS sensor", "error starting");
  } else {
    lcdMessage("SPS sensor", "started");
  }

  for (int i=0; i<HISTORY_BUFFER; i++) {
    c1_history[i] = 0.0;
  }

  IrSender.begin(IRLEDPIN);

  connectToWiFi();

  delay(1000);

  attachInterrupt(digitalPinToInterrupt(PASIRPIN), passiveIRInt, RISING);
  attachInterrupt(digitalPinToInterrupt(OVERRIDESWPIN), overrideSwitchInt, FALLING);
  attachInterrupt(digitalPinToInterrupt(CLEANSWPIN), cleanSwitchInt, FALLING);

  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  lcdMessage("Initialisation complete");
}

void format_val(char *s, double val, int width, int padding) {
  const int digits = width-padding;
  char *s_in = s;
  for (int i=0; i<padding; i++) {
    *s++ = ' ';
  }
  s = s_in;
  if (digits == 5) {
    if (val >= 100000) {
      strcpy(s, "99999");
    } else if (val >= 1000.0) {
      sprintf(s, "%5d", (int)val);
    } else {
      dtostrf(val, digits, 1, s);
    }
  } else {
    *s = '?';
  }
  s[width] = '\0';
}

void writeBar(LiquidCrystal_I2C lcd, int col, int row, float val, int width) {
  if (val > width) {
    for (int i=0; i<width-1; i++) {
      lcd.setCursor(col+i, row);
      lcd.write(5);
    }
    lcd.setCursor(col+width-1, row);
    lcd.write(6);
    return;
  }
  for (int i=0; i<width; i++) {
    lcd.setCursor(col+i, row);
    if (val >= 0.8) {
      lcd.write(5);
    } else {
      lcd.write((uint8_t)(val * 5.0 + 0.8));
    }
    if (--val < 0.0) {
      val = 0.0;
    };
  }
}

void extractor_fan(int op) {
  uint64_t *data;
  switch (op) {
    case EXT_UP:
      if (ext_fan_speed < 3) {
        data = rawDataOn;
        Serial.print("Fan ");
        Serial.print(ext_fan_speed);
        ext_fan_speed++;
        Serial.print(" -> ");
        Serial.println(ext_fan_speed);
      } else {
        data = NULL;
      }
      break;
    case EXT_OFF:
      data = rawDataOff;
      Serial.print("Fan ");
      Serial.print(ext_fan_speed);
      Serial.println(" -> 0");
      ext_fan_speed = 0;
      break;
    default:
      data = NULL;
      break;
  }
  if (data) {
    digitalWrite(INDLEDPIN, HIGH);
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Controlling AC400");
    char output[20];
    sprintf(output, "Fan speed %d/3", ext_fan_speed);
    lcdMessage(output);
    IrSender.sendPulseDistanceWidthFromArray(38, 8750, 4350, 700, 1650, 1150, 500, data, 119, PROTOCOL_IS_LSB_FIRST, 100, 0);
    delay(100);
    digitalWrite(INDLEDPIN, LOW);
  }
}

void readingsCallback() {
  checkWiFiConnection();

  char *sizes[] = {"C1", "C2.5", "C4", "C10"};
  for (int i=0; i<4; i++) {
    Serial.print(sizes[i]);
    Serial.print("=");
    Serial.print(particle_counts[i]);
    Serial.print(" ");
  }
  Serial.print("temp=");
  Serial.print(temperature);
  Serial.print("°C rh=");
  Serial.print(relative_humidity);
  Serial.print("% fan=");
  Serial.print(ext_fan_speed);
  Serial.println("");

  for (int i=0; i<HISTORY_BUFFER; i++) {
    Serial.print(c1_history[i]);
    Serial.print(" ");
  }
  Serial.println("");
}

void passiveIRInt() {
  passive_ir = 1;
  digitalWrite(INDLEDPIN, HIGH);
}

void overrideSwitchInt() {
  override_switch = 1;
}

void cleanSwitchInt() {
  clean_switch = 1;
}

// Deal with millis rolling overy ~50 days
int inTimerange(unsigned long time, unsigned long start, unsigned long end) {
  if (end > start) {
    return start < time < end;
  }

  // The timer has rolled
  if (start > end) {
    return time > start || time < end;
  }

  // They're equal
  return 0;
}

void loop() {
  struct sps30_measurement m;
  char serial[SPS30_MAX_SERIAL_LEN];
  uint16_t data_ready;
  int16_t ret;
  const char *keys[] = {"  1", "2.5", "  4", " 10"};
  const unsigned long now = millis();

  do {
    ret = sps30_read_data_ready(&data_ready); 
    if (ret < 0) {
      Serial.print("error reading data-ready flag: ");
      Serial.println(ret);
      lcd.setCursor(0, 0);
      lcd.print('SPS30 ERROR ');
    } else if (!data_ready) {
      Serial.print("data not ready, no new measurement available\n");
      lcd.setCursor(0, 0);
      lcd.print('SPS30 ERROR ');
    } else {
      break;
    }
    delay(100); /* retry in 100ms */
  } while (1);

  ret = sps30_read_measurement(&m);
  if (ret < 0) {
    Serial.println("Error reading SPS30");
    for (int i=0; i<4; i++) {
      particle_counts[i] = NAN;
    }
  } else {
    particle_counts[0] = m.nc_1p0 - m.nc_0p5;
    particle_counts[1] = m.nc_2p5 - m.nc_1p0;
    particle_counts[2] = m.nc_4p0 - m.nc_2p5;
    particle_counts[3] = m.nc_10p0 - m.nc_4p0;

    for (int i=0; i<3; i++) {
      char output[21];
      lcd.setCursor(0, i);
      strncpy(output, keys[i], 5);
      output[3] = ' ';
      format_val(output+4, particle_counts[i], 5, 0);
      output[9] = ' ';
      output[10] = '\0';
      lcd.print(output);
      writeBar(lcd, 10, i, particle_counts[i], 10);
    }
  }

  int chk = DHT.read22(DHTPIN);
  switch (chk) {
    case DHTLIB_OK:
        break;
    case DHTLIB_ERROR_CHECKSUM:
        Serial.println("DHT22 checksum error");
        break;
    case DHTLIB_ERROR_TIMEOUT:
        Serial.println("DHT22 time out error");
        break;
    default:
        Serial.println("DHT22 unknown error,\t");
        break;
  }
  if (chk == DHTLIB_OK) {
    temperature = DHT.getTemperature();
    relative_humidity = DHT.getHumidity();
  }

  char output[21] = "Temp                ";
  format_val(output+4, temperature, 5, 0);
  char *deg = " C ";
  strcat(output, deg);
  output[9] = char(223);
  const char fan_levels[4] = {'-', 'L', 'M', 'H'};
  char fan = fan_levels[ext_fan_speed];
  if (inTimerange(now, fan_override_start, fan_override_end)) {
    fan = '+';
  } else if (!inTimerange(now, pir_time, pir_time + PIR_WINDOW)) {
    fan = '_';
  }
  sprintf(output+12, "%2d%%RH F%c", int(relative_humidity + 0.5), fan);
  lcd.setCursor(0, 3);
  lcd.print(output);

  float c1 = particle_counts[0];


  unsigned int minuites = (now / MILLIS_MINUTE);
  //unsigned int next_minute = (minuites + 1) % HISTORY_BUFFER;
  unsigned int this_minute = (minuites) % HISTORY_BUFFER;
  c1_history[this_minute] = c1 > c1_history[this_minute] ? c1 : c1_history[this_minute];

  /* Find the next-to maximum C1 values over the last HISTORY_MINUTES, and use
     that, to give some hysteresis in the downward direction, to stop the fan
     speed oscillating.
  */
  float c1_hyster = 0.0;
  float c1_hyster2 = 0.0;
  for (int i=0; i<HISTORY_MINUTES; i++) {
    /* Deal with C++'s annoying modulus */
    int pos = (this_minute + HISTORY_BUFFER - i) % HISTORY_BUFFER;
    if (c1_history[pos] > c1_hyster) {
      c1_hyster2 = c1_hyster;
      c1_hyster = c1_history[pos];
    }
  }

  int target_fan_speed = 0;
  if (c1_hyster2 > 15.0) {
    target_fan_speed = 3;
  } else if (c1_hyster2 > 10.0) {
    target_fan_speed = 2;
  } else if (c1_hyster2 > 3.0) {
    target_fan_speed = 1;
  }

  if (inTimerange(now, fan_override_start, fan_override_end)) {
    target_fan_speed = max(fan_override, target_fan_speed);
  } else if (target_fan_speed > 0 && !inTimerange(now, pir_time, pir_time + PIR_WINDOW)) {
    //lcdMessage("Particles high, but", "no PIR in last hour");
    target_fan_speed = 0;
  }
 
  /* Rely on looping round to bring it to the right level eventually */
  if (target_fan_speed > ext_fan_speed) {
    extractor_fan(EXT_UP);
  } else if (target_fan_speed < ext_fan_speed) {
    extractor_fan(EXT_OFF);
  }

  if (now > next_timer) {
    next_timer += MILLIS_MINUTE;
    readingsCallback();
  }

  if (passive_ir) {
    digitalWrite(INDLEDPIN, HIGH);
    Serial.println("Passive IR triggered");
    pir_time = now;
    passive_ir = 0;
    delay(100);
    digitalWrite(INDLEDPIN, LOW);
  }

  if (override_switch) {
    override_switch = 0;
    digitalWrite(INDLEDPIN, HIGH);
    if (inTimerange(now, fan_override_start, fan_override_end)) {
      lcdMessage("Fan override", "cancel");
      fan_override = 0;
      fan_override_start = now;
      fan_override_end = now;
      delay(200);
      digitalWrite(INDLEDPIN, LOW);
    } else {
      lcdMessage("Fan override");
      fan_override = 2;
      fan_override_start = now + 1;
      fan_override_end = now + 3600 * 1000;
      delay(200);
      digitalWrite(INDLEDPIN, LOW);
    }
  }

  if (digitalRead(CLEANSWPIN) == 0) {
    lcdMessage("SPS30 cleaning");
    sps30_start_manual_fan_cleaning();
  }

  delay(2000);
}

