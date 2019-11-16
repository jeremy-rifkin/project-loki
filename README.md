# Project Loki

The goal of this project was to create a system which exploits some of the design flaws in the
iClicker classroom polling system such as flooding the base station with bogus answers and changing
other student's submissions. The software utilizes a 900 MHz packet radio capable microprocessor (in
my case the Adafruit Feather M0 RFM69HCW) and is built on top of the work done by
[@wizard97, @orangeturtle739, and others](https://github.com/wizard97/iSkipper) to reverse engineer
the iClicker protocol.

# Project Architecture

The code is written such that the microprocessor does all the work and commands are issued to the
board via Serial. Commands should end in a `\n` (which is default in the Arduino IDE).

The code attempts to keep track of every answer that is submitted to the base station. The system
is likely to miss a number of packets as it can't receive and transmit at the same time. A more
optimal solution would be to use 2 feathers: one for intercepting and one for transmitting.

# License

[MIT](/LICENSE)

# Disclaimer

The use of this device and software may be frowned upon. Depending on how you use this device, it
may be considered academically dishonest or harmful to the other students in your class/lecture. I
am not liable for anything you do using this device or software.

Also: I made this because I could and thought it would be interesting. Not because I intend to make
extensive use of it myself it myself.

I have three personal rules for using this: 1) I don't spoof attendance, 2) I don't use it to cheat
by looking at how other people are voting, and 3) I don't mess with other peoples' answers on polls
which will count for a grade.
