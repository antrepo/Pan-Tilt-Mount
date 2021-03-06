/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE
*
* Code written by isaac879
*/
/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

#include <stdio.h>
#include <Windows.h>
#include <Xinput.h>
#include <iostream>
#include <string>
#include <fstream>

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define UP_BUTTON 1
#define DOWN_BUTTON 2
#define LEFT_BUTTON 4
#define RIGHT_BUTTON 8
#define MENU_BUTTON 16
#define VIEW_BUTTON 32
#define L_BUTTON 64
#define R_BUTTON 128
#define LB_BUTTON 256
#define RB_BUTTON 512
#define A_BUTTON 4096
#define B_BUTTON 8192
#define X_BUTTON 16384
#define Y_BUTTON 32768

#define INSTRUCTION_BYTES_PAN_SPEED 1
#define INSTRUCTION_BYTES_TILT_SPEED 2
#define INSTRUCTION_BYTES_PAN_TILT_SPEED 3

#define INPUT_DEADZONE 1000

//Flags
bool portOpenFlag = false; //The flag used to determin if the serial port for the Arduino Nano is opened
HANDLE hSerial;// Serial handle

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

byte getBytesFromInt(short sourceInt, unsigned int byteNumber) {//Gets a byte from an int. byte number should be 0 for the 8 least signigicant bits or 1 for the most fignificant bits
	byte byteValue = (sourceInt >> (8 * byteNumber)) & 0xFF;
	return byteValue;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void openSerial(LPCSTR port){
	if (portOpenFlag == false){//If the port is not already open
		hSerial = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);//Set the Com port to the correct arduino one

		if (hSerial == INVALID_HANDLE_VALUE){
			std::cout << "Error: invalid com port handle. Attempted to open: " << port << std::endl;
		}
		else{
			portOpenFlag = true;
			std::cout << "Successfully opened: " << port << std::endl;
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int closeSerial(){
	if (portOpenFlag == true){//If the port is open
		if (CloseHandle(hSerial) == 0){
			std::cout << "Error\n" << stderr << std::endl;
			return -1;
		}
		portOpenFlag = false;
		printf("Serial port closed...\n");
		return 1;
	}
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int serialConnect(LPCSTR portName){
	std::cout << "Attempting to open serial port " << portName << std::endl;
	openSerial(portName);//Open the serial port

	// Declare variables and structures
	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);//why do i need this?
	if (GetCommState(hSerial, &dcbSerialParams) == 0){
		closeSerial();
		return -1;
	}
	else{
		dcbSerialParams.BaudRate = CBR_57600; //Set device parameters (57600 baud, 1 start bit, 1 stop bit, no parity)
		dcbSerialParams.ByteSize = 8;
		dcbSerialParams.StopBits = ONESTOPBIT;
		dcbSerialParams.Parity = NOPARITY;
		dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE; //Stops the Arduino resetting after connecting
	}

	if(SetCommState(hSerial, &dcbSerialParams) == 0){
		closeSerial();
		return -1;
	}

	// Set COM port timeout settings
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	if(SetCommTimeouts(hSerial, &timeouts) == 0){
		closeSerial();
		return 1;
	}
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int serialWrite(char data[], int lengthOfArray){
	DWORD bytes_written;
	if (!WriteFile(hSerial, data, lengthOfArray, &bytes_written, NULL)) {
		closeSerial();
		return -1;
	}
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void printIncomingData(void){
	COMSTAT status;
	DWORD errors;
	DWORD bytesRead;//Number of bytes that have been read

	while(1){
		ClearCommError(hSerial, &errors, &status); //Use the ClearCommError function to get status info on the Serial port	
		if(status.cbInQue == 0) break;

		char* dataBuffer = new char[status.cbInQue + 1]; //buffer the size of the data available to be read
		dataBuffer[status.cbInQue] = '\0'; //adds a null terminator to the end of the buffer

		if(ReadFile(hSerial, dataBuffer, status.cbInQue, &bytesRead, NULL)){ //Try to read the require number of chars, and return the number of read bytes on success
			std::cout << dataBuffer;// << std::endl;
		}
		delete[] dataBuffer;
		Sleep(5); //Wait for 2ms before checking if more data has come in
	} 
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void sendCommand(char command){
	char data[] = {command}; //Data array to send
	serialWrite(data, sizeof(data));
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

void sendCharArray(char *array){
	serialWrite(array, (int)strlen(array));
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int sendStepSpeed(int command, short num){
	char data[3]; //Data array to send

	data[0] = command;
	data[1] = getBytesFromInt(num, 1);//Gets the 8 MSBs
	data[2] = getBytesFromInt(num, 0);//Gets the 8 LSBs

	serialWrite(data, sizeof(data)); //Send the command and the 4 bytes of data

	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int sendPanTiltStepSpeed(int command, int num){
	char data[5]; //Data array to send

	data[0] = command;
	data[1] = (num >> (8 * 3)) & 0xFF; //Gets the most significant byte
	data[2] = (num >> (8 * 2)) & 0xFF; //Gets the second most significant byte
	data[3] = (num >> (8 * 1)) & 0xFF; //Gets the third most significant byte
	data[4] = num & 0xFF; //Gets the least significant byte

	serialWrite(data, sizeof(data)); //Send the command and the 4 bytes of data

	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

std::string getPortName(std::string filePath){
	std::string line;
	std::cout << "Reading port name from: " << filePath << std::endl;
	std::ifstream readfile(filePath);

	if(readfile.is_open()){
		if (getline(readfile, line)){
			std::cout << "Port read from the file: " << line << std::endl;
		}
		readfile.close();
	}
	else {
		std::cout << "Error: Unable to open serial_port.txt" << std::endl;
	}
	return line;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int panTiltInit(void){
	//Connects to the com port that the Arduino is connected to.
	for(int i = 0; (i < 5) && (serialConnect(getPortName("D:\\Documents\\Projects\\Pan Tilt Mount\\serial_port.txt").c_str()) != 0 ); i++){
		Sleep(1000);
		if(i == 4){
			std::cout << "Error: Unable to open serial port after 5 attempts..." << std::endl;
			return -1;
		}
	}
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/

int main(void){
	panTiltInit();

	DWORD dwResult;   
	WORD lastwButtons = 0;
	DWORD lastDwPacketNumber = 0;
	for(DWORD i = 0; i < XUSER_MAX_COUNT; i++){
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));
	
		dwResult = XInputGetState(i, &state); //Simply get the state of the controller from XInput.
		if(dwResult == ERROR_SUCCESS){ //Controller is connected 
			printf("Connected.\n");
			while(1){
				printIncomingData();
				XInputGetState(i, &state);
				//printf("dw packet number: %d\n", state.dwPacketNumber);
				if(state.dwPacketNumber != lastDwPacketNumber){

					float RX = state.Gamepad.sThumbRX; //Get right analog stick X value
					if(abs(RX) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE){
						RX = 0;
					}
					else if(RX > 0){
						RX -= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
					}
					else if(RX < 0){
						RX += XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
					}
					short RXShort = -(short(((float)RX * 2000.0) / (32767.0 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE))); //Normalise to a range of -2000 to 2000
					
					float RY = state.Gamepad.sThumbRY; //Get right analog stick Y value
					if(abs(RY) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE){
						RY = 0;
					}
					else if(RY > 0){
						RY -= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
					}
					else if(RY < 0){
						RY += XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
					}
					short RYShort = -(short(((float)RY * 2000.0) / (32767.0 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE))); //Normalise to a range of -2000 to 2000

					int RXRYInt = (RXShort << 16) + RYShort; //Combine both the X and Y 2 byte short values into one 4 byte int value
					sendPanTiltStepSpeed(INSTRUCTION_BYTES_PAN_TILT_SPEED, RXRYInt); //send the combned X and Y values
					Sleep(10);

					if((lastwButtons & UP_BUTTON) < (state.Gamepad.wButtons & UP_BUTTON)){
						printf("Up: %d \n", (state.Gamepad.wButtons & UP_BUTTON));
						sendCharArray((char *)"["); //Up first element
					}
					if((lastwButtons & DOWN_BUTTON) < (state.Gamepad.wButtons & DOWN_BUTTON)){
						printf("Down: %d \n", ((state.Gamepad.wButtons & UP_BUTTON) >> 1));
						sendCharArray((char *)"]"); //down last element
					}
					if((lastwButtons & LEFT_BUTTON) < (state.Gamepad.wButtons & LEFT_BUTTON)){
						printf("Left: %d \n", ((state.Gamepad.wButtons & LEFT_BUTTON) >> 2));
						sendCharArray((char *)"<"); //left step back
					}
					if((lastwButtons & RIGHT_BUTTON) < (state.Gamepad.wButtons & RIGHT_BUTTON)){
						printf("Right: %d \n", ((state.Gamepad.wButtons & RIGHT_BUTTON) >> 3));
						sendCharArray((char *)">"); //right step forwards
					}
					if((lastwButtons & MENU_BUTTON) < (state.Gamepad.wButtons & MENU_BUTTON)){
						printf("Menu: %d \n", ((state.Gamepad.wButtons & MENU_BUTTON) >> 4));
						sendCharArray((char *)"A"); //Menu home
					}
					if((lastwButtons & VIEW_BUTTON) < (state.Gamepad.wButtons & VIEW_BUTTON)){
						printf("View: %d \n", ((state.Gamepad.wButtons & VIEW_BUTTON) >> 5));
						sendCharArray((char *)";1"); //views execute
					}
					if((lastwButtons & L_BUTTON) < (state.Gamepad.wButtons & L_BUTTON)){
						printf("L: %d \n", ((state.Gamepad.wButtons & L_BUTTON) >> 6));
						sendCharArray((char *)"m4"); //step mode
					}
					if((lastwButtons & R_BUTTON) < (state.Gamepad.wButtons & R_BUTTON)){
						printf("R: %d \n", ((state.Gamepad.wButtons & R_BUTTON) >> 7));
						sendCharArray((char *)"m16"); //sixteenth step mode
					}
					if((lastwButtons & LB_BUTTON) < (state.Gamepad.wButtons & LB_BUTTON)){
						printf("LB: %d \n", ((state.Gamepad.wButtons & LB_BUTTON) >> 8));
						sendCharArray((char *)"D300"); //add delay of 300ms
					}
					if((lastwButtons & RB_BUTTON) < (state.Gamepad.wButtons & RB_BUTTON)){
						printf("RB: %d \n", ((state.Gamepad.wButtons & RB_BUTTON) >> 9));
						sendCharArray((char *)"c"); //RB Shutter
					}
					if((lastwButtons & A_BUTTON) < (state.Gamepad.wButtons & A_BUTTON)){
						printf("A: %d \n", ((state.Gamepad.wButtons & A_BUTTON) >> 12));
						sendCharArray((char *)"#"); //A save position
					}
					if((lastwButtons & B_BUTTON) < (state.Gamepad.wButtons & B_BUTTON)){
						printf("B: %d \n", ((state.Gamepad.wButtons & B_BUTTON) >> 13));
						sendCharArray((char *)"C"); //B clear array
					}
					if((lastwButtons & X_BUTTON) < (state.Gamepad.wButtons & X_BUTTON)){
						printf("X: %d \n", ((state.Gamepad.wButtons & X_BUTTON) >> 14));
						sendCharArray((char *)"E"); //X edit position
					}
					if((lastwButtons & Y_BUTTON) < (state.Gamepad.wButtons & Y_BUTTON)){
						printf("Y: %d \n", ((state.Gamepad.wButtons & Y_BUTTON) >> 15));
						sendCharArray((char *)"R"); //Y status
					}
					//printf("LX: %d \t", state.Gamepad.sThumbLX);
					//printf("LY: %d \t", state.Gamepad.sThumbLY);
					//printf("RX: %d \t", state.Gamepad.sThumbRX);
					//printf("RY: %d \n", state.Gamepad.sThumbRY);
					//printf("LT: %d \t", state.Gamepad.bLeftTrigger);
					//printf("RT: %d \n", state.Gamepad.bRightTrigger);
					//printf("wButtons %d \n", state.Gamepad.wButtons);
					lastwButtons = state.Gamepad.wButtons;
					lastDwPacketNumber = state.dwPacketNumber;
				}
			}
		}
		else{ //Controller is not connected 
			printf("Not Connected.\n");
		}
	}
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------------------------------*/
