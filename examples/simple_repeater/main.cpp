#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#include "MyMesh.h"

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(display);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];

#if defined(WIRELESS_PAPER)
static const unsigned long SERIAL_CONSOLE_BOOT_WINDOW_MS = 120000;

static bool shouldPollSerialConsole() {
  return command[0] || millis() < SERIAL_CONSOLE_BOOT_WINDOW_MS;
}
#else
static bool shouldPollSerialConsole() {
  return true;
}
#endif

// For power saving
unsigned long lastActive = 0; // mark last active time
unsigned long nextSleepinSecs = 120; // next sleep in seconds. The first sleep (if enabled) is after 2 minutes from boot

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
static unsigned long userBtnDownAt = 0;
#define USER_BTN_HOLD_OFF_MILLIS 1500
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  the_mesh.beginObserverHealthBoot();
  the_mesh.setObserverHealthPhase(64);
  board.begin();
  the_mesh.setObserverHealthPhase(65);

#if defined(MESH_DEBUG) && defined(NRF52_PLATFORM)
  // give some extra time for serial to settle so
  // boot debug messages can be seen on terminal
  delay(5000);
#endif

  // For power saving
  lastActive = millis(); // mark last active time since boot

#ifdef DISPLAY_CLASS
  the_mesh.setObserverHealthPhase(66);
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
  the_mesh.setObserverHealthPhase(67);
#endif

  the_mesh.setObserverHealthPhase(68);
  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    halt();
  }
  the_mesh.setObserverHealthPhase(69);

  fast_rng.begin(radio_get_rng_seed());
  the_mesh.setObserverHealthPhase(70);

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif
  the_mesh.setObserverHealthPhase(71);
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }
  the_mesh.setObserverHealthPhase(72);

#ifdef ENABLE_BOOT_ID_PRINT
  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();
#endif

  command[0] = 0;

  the_mesh.setObserverHealthPhase(73);
  sensors.begin();

  the_mesh.setObserverHealthPhase(74);
  the_mesh.begin(fs);
  the_mesh.setObserverHealthPhase(75);

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION, &the_mesh);
#endif
  the_mesh.setObserverHealthPhase(76);

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif
  the_mesh.setObserverHealthPhase(0);
}

void loop() {
  the_mesh.setObserverHealthPhase(58);
  int len = strlen(command);
  if (shouldPollSerialConsole()) {
    while (Serial.available() && len < sizeof(command)-1) {
      char c = Serial.read();
      if (c != '\n') {
        command[len++] = c;
        command[len] = 0;
        Serial.print(c);
      }
      if (c == '\r') break;
    }
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    Serial.print('\n');
    command[len - 1] = 0;  // replace newline with C string null terminator
#if defined(ENABLE_DISPLAY_DUMP) && defined(DISPLAY_CLASS)
    if (strncmp(command, "screen.dump", 11) == 0 && (command[11] == 0 || command[11] == ' ')) {
      char* arg = command + 11;
      while (*arg == ' ') arg++;
      if (*arg) {
        int screen = atoi(arg);
        if (!ui_task.renderScreenForDump((uint8_t)screen)) {
          Serial.println("ERR bad screen");
        } else {
          display.dumpPBM(Serial);
        }
      } else {
        display.dumpPBM(Serial);
      }
      command[0] = 0;
    } else
#endif
    {
    the_mesh.setObserverHealthPhase(59);
    the_mesh.setObserverHealthMarker(6);
    unsigned long cli_started = millis();
    char reply[160];
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
    the_mesh.noteObserverTiming(6, millis() - cli_started);
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
    }
  }

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
  // Hold the user button to power off the SenseCAP Solar repeater.
  int btnState = digitalRead(PIN_USER_BTN);
  if (btnState == LOW) {
    if (userBtnDownAt == 0) {
      userBtnDownAt = millis();
    } else if ((unsigned long)(millis() - userBtnDownAt) >= USER_BTN_HOLD_OFF_MILLIS) {
      Serial.println("Powering off...");
      board.powerOff();  // does not return
    }
  } else {
    userBtnDownAt = 0;
  }
#endif

  the_mesh.setObserverHealthPhase(60);
  the_mesh.loop();
  the_mesh.setObserverHealthPhase(61);
  sensors.loop();
#ifdef DISPLAY_CLASS
  the_mesh.setObserverHealthPhase(62);
  the_mesh.setObserverHealthMarker(5);
  unsigned long display_started = millis();
  ui_task.loop();
  the_mesh.noteObserverTiming(5, millis() - display_started);
#endif
  the_mesh.setObserverHealthPhase(63);
  rtc_clock.tick();
  the_mesh.setObserverHealthPhase(0);

  if (the_mesh.getNodePrefs()->powersaving_enabled && !the_mesh.hasPendingWork()) {
    #if defined(NRF52_PLATFORM)
    board.sleep(1800); // nrf ignores seconds param, sleeps whenever possible
    #else
    if (the_mesh.millisHasNowPassed(lastActive + nextSleepinSecs * 1000)) { // To check if it is time to sleep
      board.sleep(1800);             // To sleep. Wake up after 30 minutes or when receiving a LoRa packet
      lastActive = millis();
      nextSleepinSecs = 5;  // Default: To work for 5s and sleep again
    } else {
      nextSleepinSecs += 5; // When there is pending work, to work another 5s
    }
    #endif
  }
}
