// Copyright 2020 <Zach Whitehead>
#include <Arduino.h>
#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
#endif

#include "eppgBuzzer.h"

// initialize the buzzer
void initBuzz() {
  pinMode(BUZZER_PIN, OUTPUT);
}

#ifdef RP_PIO
// non-blocking tone function that uses second core
void playNote(uint16_t note, uint16_t duration) {
    STR_NOTE noteData;
    // fifo uses 32 bit messages so package up the note and duration
    uint32_t note_msg;
    noteData.duration = duration;
    noteData.freq = note;

    memcpy((uint32_t*)&note_msg, &noteData, sizeof(noteData));
    rp2040.fifo.push_nb(note_msg);  // send note to second core via fifo queue
}
#else
// blocking tone function that delays for notes
void playNote(uint16_t note, uint16_t duration) {
  // quarter note = 1000 / 4, eigth note = 1000/8, etc.
  tone(BUZZER_PIN, note);
  delay(duration);  // to distinguish the notes, delay between them
  noTone(BUZZER_PIN);
}
#endif

bool playMelody(uint16_t melody[], int siz) {
  if (!ENABLE_BUZ) { return false; }
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    // quarter note = 1000 / 4, eigth note = 1000/8, etc.
    int noteDuration = 125;
    playNote(melody[thisNote], noteDuration);
  }
  return true;
}
