#ifndef INC_EPPG_BUZZER_H_
#define INC_EPPG_BUZZER_H_

#include <stdint.h>

void   initBuzz();
bool   playMelody(uint16_t melody[], int siz);
void   playNote(uint16_t note, uint16_t duration);
void   initVibe();
void   vibrateNotify();
bool   runVibe(unsigned int sequence[], int siz);

#endif