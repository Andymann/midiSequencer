/*
  MIDI clock receiver
  Arduino Leonardo
 */
#include <Smooth.h>
#include "MIDIUSB.h"
#include <Adafruit_NeoPixel.h>

#define LEDDIMM 4      

#define TIMINGOFFSETMS -100
#define PPQN 24
#define SMOOTHED_SAMPLE_SIZE  24
#define BEATRESOLUTION 4
#define SEQUENCERSTEPS 8
#define DISPLAYABLE_STEPS 16
#define DISPLAYABLE_BANKS 4
//uint8_t iSequencerStepIndex = 0;

#define LEDPIN 2
#define NUMPIXELS 20

#define MODE_PLAY 1
#define MODE_RECORD 2
uint8_t iMode = MODE_RECORD;

/*
typedef struct{
      int red;
      int green;
      int blue;
}  color;

color bankColor[4];
*/


typedef struct{
  bool  bHasData = false;
  uint8_t channel = -1;
  uint8_t controller = -1;
  uint8_t pitch = -1;
  uint8_t velocity = -1; //used for value when controller != -1
  // gate
  // probability
  // ...
} SequencerStep;

SequencerStep sequencerStep[SEQUENCERSTEPS];

Adafruit_NeoPixel pixels(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);
Smooth  averageTimediff(SMOOTHED_SAMPLE_SIZE);  // Smoothing average object

int ppqnCounter = 0;
unsigned long previousMicros = 0;
uint8_t iStepIndex = 0; //



void setup() {
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  testPixels();
  Serial.begin(9600);
  while (!Serial) ;
  preloadPattern();
  presetClockSpeed(120);
  Serial.println("set BPM:" + String( calculateBPM(averageTimediff())));
  Serial.println("Initialization done");
}

void loop() {
  
  midiEventPacket_t rx;
  
  do {
    rx = MidiUSB.read();
    //Count pulses and send note 
    if(rx.byte1 == 0xF8){
      unsigned long currentMicros = micros();
      unsigned long tmpTimeDiff = currentMicros - previousMicros;
      if(tmpTimeDiff>3000){
        tmpTimeDiff = 3000; // 20 BPM
      }else if(tmpTimeDiff<150){
        tmpTimeDiff=150;  // 400 BPM
      }
      ++ppqnCounter; 
      averageTimediff += currentMicros - previousMicros; // Turned out to be more precise than avaraging calculated BPMs
      previousMicros = currentMicros;

      // Downbeat
      if(ppqnCounter == PPQN){
        Serial.println("BPM:" + String( calculateBPM(averageTimediff())));
        ppqnCounter = 0 ;
      };

      // Dpending on sequencer's resolution
      if(ppqnCounter%(PPQN*4/BEATRESOLUTION)==0){
        if(iStepIndex<SEQUENCERSTEPS-1){
          iStepIndex++;
        }else{
          iStepIndex=0;
        }
        //displaySequencerStep( iStepIndex );
        processSequencerStep( iStepIndex );
      }
    }

    //Clock start byte
    else if(rx.byte1 == 0xFA){
      ppqnCounter = PPQN-1; // Next TICK will trigger a downbeat
      iStepIndex = -1;
    }

    //Clock stop byte
    else if(rx.byte1 == 0xFC){
      ppqnCounter = 0;
      setPixelsOff();
    }

   else if((rx.byte1 >= 0x90) && (rx.byte1 < 0x9F)){
      //Serial.println("Note ON: " + String(rx.byte2) + " " + String(rx.byte3));
      if(iMode==MODE_RECORD){
        recordStep_Note(rx);
      }
    }

    else if((rx.byte1 >= 0xB0) && (rx.byte1 < 0xBF)){
      Serial.println("CC: " + String(rx.byte2) + " " + String(rx.byte3));
    }
    
  } while (rx.header != 0);
  
}

/*
  Incoming MIDI data are stored at the current/ nearest/ next(?) iStepIndex. 
  There's probably lots of room for errors and/ or mad cool features in this.
*/
void recordStep_Note(midiEventPacket_t pRX){
  sequencerStep[iStepIndex].bHasData = true;
  sequencerStep[iStepIndex].channel = pRX.byte1-0x90;
  sequencerStep[iStepIndex].pitch = pRX.byte2;
  sequencerStep[iStepIndex].velocity = pRX.byte3;
}

float calculateBPM(unsigned long pTimeDiff){
  float fBPM = 60000000/averageTimediff()/PPQN;
  return fBPM;
}

long calculateTimediffMS(int pBPM){
  long timeDiff = 60000000/pBPM/PPQN;
  Serial.println("TimeDiff(" + String(pBPM) + "):" + String(timeDiff));
  return long(timeDiff);
}

void processSequencerStep(int pStepIndex){
  displaySequencerStep(pStepIndex);
  if(sequencerStep[pStepIndex].bHasData){
    if(sequencerStep[pStepIndex].pitch!=-1){
      //Serial.println("OUTPUT");
      noteOn(sequencerStep[pStepIndex].channel, sequencerStep[pStepIndex].pitch, sequencerStep[pStepIndex].velocity);
      
    }
  }
}

void displaySequencerStep(int pStepIndex){
  int iStep;
  uint8_t iBank = pStepIndex / DISPLAYABLE_STEPS;
  setBankPixel(iBank);
  //if(SEQUENCERSTEPS>DISPLAYABLE_STEPS){
    iStep = pStepIndex - iBank*DISPLAYABLE_STEPS;
  //}else{
  //  iStep = pStepIndex - iBank*SEQUENCERSTEPS;
  //}
  setStepPixel( iStep );
}

void testPixels(){
  for(int i=0; i<NUMPIXELS; i++){
    pixels.setPixelColor(i, pixels.Color(0, LEDDIMM, 0));
  }
  pixels.show();
  delay(1000);
  setPixelsOff();
}

void setBankPixel(uint8_t pBank){
  for (int i=0; i<DISPLAYABLE_BANKS; i++){
    pixels.setPixelColor(DISPLAYABLE_STEPS+i, pixels.Color(0, 0, 0));
  }
  pixels.setPixelColor(DISPLAYABLE_STEPS + pBank, pixels.Color(0, 0, LEDDIMM));
  pixels.show(); 
}

void setStepPixel(int pStep){
  if(pStep==0){
    if(SEQUENCERSTEPS>DISPLAYABLE_STEPS){
      pixels.setPixelColor(DISPLAYABLE_STEPS-1, pixels.Color(0, 0, 0));
    }else{
      pixels.setPixelColor(SEQUENCERSTEPS-1, pixels.Color(0, 0, 0));
    }
  }else{
    pixels.setPixelColor(pStep-1, pixels.Color(0, 0, 0));
  }

  // Steps with recorded stuff light up differently
  if(sequencerStep[pStep].bHasData){
     pixels.setPixelColor(pStep, pixels.Color(LEDDIMM, 0, LEDDIMM));
  }else{
    pixels.setPixelColor(pStep, pixels.Color(0, LEDDIMM, 0));
  }
  pixels.show();
}

void setPixelsOff(){
  for(int i=0; i<NUMPIXELS; i++){
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
}

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();
}


/*
  Plant testdata
*/
void preloadPattern(){
  sequencerStep[0].channel = 0;
  sequencerStep[0].bHasData = true;
  sequencerStep[0].pitch = 0x24;
  sequencerStep[0].velocity = 0x64;

  sequencerStep[4].channel = 0;
  sequencerStep[4].bHasData = true;
  sequencerStep[4].pitch = 0x24;
  sequencerStep[4].velocity = 0x64;
}

void presetClockSpeed(int pBPM){
  for(uint8_t i=0; i<SMOOTHED_SAMPLE_SIZE; i++){
    averageTimediff += calculateTimediffMS(pBPM);
  }
}
