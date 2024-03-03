/*
  MIDI clock receiver
 */
#include <Smooth.h>
#include "MIDIUSB.h"
#define PPQN 24
#define  SMOOTHED_SAMPLE_SIZE  24

// Smoothing average object
Smooth  averageTimediff(SMOOTHED_SAMPLE_SIZE);

int ppqnCounter = 0;
unsigned long previousMicros = 0;

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void setup() {
  Serial.begin(9600);
  while (!Serial) ;
  Serial.println("Initialization done");
}

void loop() {
  
  midiEventPacket_t rx;
  
  do {
    rx = MidiUSB.read();
    //Count pulses and send note 
    if(rx.byte1 == 0xF8){
       ++ppqnCounter;
       
      unsigned long currentMicros = micros();
      averageTimediff += currentMicros - previousMicros; // Turned out to be more precise than avaraging calculated BPMs
      previousMicros = currentMicros;

       // Downbeat
       if(ppqnCounter == PPQN){
          Serial.println("BPM:" + String( calculateBPM(averageTimediff()) ));
          noteOn(1,48,127);
          MidiUSB.flush();      
          ppqnCounter = 0;
       };
    }
    //Clock start byte
    else if(rx.byte1 == 0xFA){
      noteOn(1,48,127);
      MidiUSB.flush();
      ppqnCounter = 0;
    }
    //Clock stop byte
    else if(rx.byte1 == 0xFC){
      noteOff(1,48,0);
      MidiUSB.flush();
      ppqnCounter = 0;
    };
    
  } while (rx.header != 0);
  
}

float calculateBPM(unsigned long pTimeDiff){
  float fBPM = 60000000/averageTimediff()/PPQN;
  return fBPM;
}
