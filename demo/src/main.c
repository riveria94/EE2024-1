#include "stdio.h"
#include "stdbool.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "light.h"
#include "temp.h"
#include "oled.h"
#include "rgb.h"
#include "led7seg.h"

uint32_t getTicks();

static void drawOled(uint8_t joyState)
{
	static int wait = 0;
	static uint8_t currX = 48;
	static uint8_t currY = 32;
	static uint8_t lastX = 0;
	static uint8_t lastY = 0;

	if ((joyState & JOYSTICK_CENTER) != 0) {
		oled_clearScreen(OLED_COLOR_BLACK);
		return;
	}

	if (wait++ < 3)
		return;

	wait = 0;

	if ((joyState & JOYSTICK_UP) != 0 && currY > 0) {
		currY--;
	}

	if ((joyState & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
		currY++;
	}

	if ((joyState & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
		currX++;
	}

	if ((joyState & JOYSTICK_LEFT) != 0 && currX > 0) {
		currX--;
	}

	if (lastX != currX || lastY != currY) {
		oled_putPixel(currX, currY, OLED_COLOR_WHITE);
		lastX = currX;
		lastY = currY;
	}
}

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);

static uint32_t notes[] = {
		2272, // A - 440 Hz
		2024, // B - 494 Hz
		3816, // C - 262 Hz
		3401, // D - 294 Hz
		3030, // E - 330 Hz
		2865, // F - 349 Hz
		2551, // G - 392 Hz
		1136, // a - 880 Hz
		1012, // b - 988 Hz
		1912, // c - 523 Hz
		1703, // d - 587 Hz
		1517, // e - 659 Hz
		1432, // f - 698 Hz
		1275, // g - 784 Hz
};

static void playNote(uint32_t note, uint32_t durationMs) {

	uint32_t t = 0;

	if (note > 0) {

		while (t < (durationMs*1000)) {
			NOTE_PIN_HIGH();
			Timer0_us_Wait(note / 2);
			//delay32Us(0, note / 2);

			NOTE_PIN_LOW();
			Timer0_us_Wait(note / 2);
			//delay32Us(0, note / 2);

			t += note;
		}

	}
	else {
		Timer0_Wait(durationMs);
		//delay32Ms(0, durationMs);
	}
}

static uint32_t getNote(uint8_t ch)
{
	if (ch >= 'A' && ch <= 'G')
		return notes[ch - 'A'];

	if (ch >= 'a' && ch <= 'g')
		return notes[ch - 'a' + 7];

	return 0;
}

static uint32_t getDuration(uint8_t ch)
{
	if (ch < '0' || ch > '9')
		return 400;

	/* number of ms */

	return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
	switch (ch) {
	case '+':
		return 0;
	case ',':
		return 5;
	case '.':
		return 20;
	case '_':
		return 30;
	default:
		return 5;
	}
}

static void playSong(uint8_t *song) {
	uint32_t note = 0;
	uint32_t dur  = 0;
	uint32_t pause = 0;

	/*
	 * A song is a collection of tones where each tone is
	 * a note, duration and pause, e.g.
	 *
	 * "E2,F4,"
	 */

	while(*song != '\0') {
		note = getNote(*song++);
		if (*song == '\0')
			break;
		dur  = getDuration(*song++);
		if (*song == '\0')
			break;
		pause = getPause(*song++);

		playNote(note, dur);
		//delay32Ms(0, pause);
		Timer0_Wait(pause);

	}
}

static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_GPIO(void)
{
	// Initialize button
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1<<31, 0);
}

void pinsel_uart3(void){

	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);
}

void init_uart(void){

	UART_CFG_Type uartCfg;
	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;
	//pin select for uart3;
	pinsel_uart3();
	//supply power & setup working parameters for uart3
	UART_Init(LPC_UART3, &uartCfg);
	//enable transmit for uart3
	UART_TxCmd(LPC_UART3, ENABLE);
}

uint32_t ticks=0;

// This function is called every 1us
void SysTick_Handler(void)
{
	ticks++;
}

void EINT3_IRQHandler (void)
{
	if ((LPC_GPIOINT -> IO2IntStatF >> 10) & 0x1){
		// printf("interrupt");
		LPC_GPIOINT -> IO2IntClr |= 1<<10;
	}
	if ((LPC_GPIOINT -> IO2IntStatF >> 5) & 0x1){
		// printf("interrupt2");
		light_clearIrqStatus();
		LPC_GPIOINT -> IO2IntClr |= 1<<5;
	}
}

uint32_t getTicks(){
	return ticks / 1000;
}

#define LIGHT_LOW_WARNING       200
#define UPDATE_INTERVAL_FIRST   5
#define UPDATE_INTERVAL_SECOND  10
#define UPDATE_INTERVAL_THRID   15
#define TEMPERATURE_OFFSET      10

static char* msg = NULL;

uint32_t sTicks;
int songFlag = 0;

int32_t xoff = 0;
int32_t yoff = 0;
int32_t zoff = 0;

int8_t x = 0;
int8_t y = 0;
int8_t z = 0;

int8_t xold = 0;
int8_t yold = 0;
int8_t zold = 0;

uint8_t dir = 1;
uint8_t wait = 0;

uint8_t state = 0;

uint32_t light = 0;
double temp;
char sens_value_light[5];
char sens_value_accX[5];
char sens_value_accY[5];
char sens_value_accZ[5];
char sens_value_temp[5];

uint8_t data = 0;
uint32_t len = 0;
uint8_t line[64];

uint8_t btn1 = 1;

int startTime = 0;

uint32_t debounceTime = 0;
uint32_t lastDebounceTime = 0;
uint32_t debounceDelay = 50;

void acc_calibrate() {

	/*
	 * Assume base board in zero-g position when reading first value.
	 */
	acc_read(&x, &y, &z);
	xoff = 0-x;
	yoff = 0-y;
	zoff = 64-z;

}

void speaker_init() {

	/* ---- Speaker ------> */

	GPIO_SetDir(2, 1<<0, 1);
	GPIO_SetDir(2, 1<<1, 1);

	GPIO_SetDir(0, 1<<27, 1);
	GPIO_SetDir(0, 1<<28, 1);
	GPIO_SetDir(2, 1<<13, 1);
	GPIO_SetDir(0, 1<<26, 1);

	GPIO_ClearValue(0, 1<<27); //LM4811-clk
	GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
	GPIO_ClearValue(2, 1<<13); //LM4811-shutdn

}

void oled_calibrate() {

	/* <---- Setup display ----> */
	oled_clearScreen(OLED_COLOR_WHITE);
	GPIO_SetValue( 2, (1<<1) );

	oled_putString(2, 2, "Light : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(2, 12, "Acc_X : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(2, 22, "Acc_Y : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(2, 32, "Acc_Z : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(2, 42, "Temp :  ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(2, 52, "MONITOR", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

}

void joystick_start() {

	/* ####### Joystick and OLED  ###### */
	state = joystick_read();
	if (state != 0)
		drawOled(state);

}

void initialiseSystem() {

	// SystemTick clock configuration
	SysTick_Config(SystemCoreClock / 1000000);  // every 1us

	init_i2c();
	init_ssp();
	init_GPIO();
	init_uart();

	rgb_init();
	pca9532_init();
	joystick_init();
	acc_init();
	light_enable();
	temp_init(*getTicks);
	oled_init();
	led7seg_init();

	acc_calibrate();
	speaker_init();
	oled_calibrate();

}

void sevenSegmentDisplay_start() {

	/* <----- Seven Segment Display -----> */
	sTicks = ticks / 1000000 % 16;
	if(0 <= sTicks && sTicks < 10) {
		led7seg_setChar(sTicks%16 + '0', FALSE);
	} else if(10 <= sTicks && sTicks < 16) {
		led7seg_setChar(sTicks%16 + '7', FALSE);
	}

}

void cuteSensor_start() {

	// Stores the value of the previous reading of the accelerometer
	xold = x;
	yold = y;
	zold = z;

	acc_read(&x, &y, &z);
	x = x+xoff;
	y = y+yoff;
	z = z+zoff;

	temp = (double) temp_read() / 10;

	light = light_read();

}

void updateOledDisplay() {

	sprintf(sens_value_light, "%d", light);
	oled_putString(50, 2, sens_value_light, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	sprintf(sens_value_accX, "%d", x);
	oled_putString(50, 12, sens_value_accX, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	sprintf(sens_value_accY, "%d", y);
	oled_putString(50, 22, sens_value_accY, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	sprintf(sens_value_accZ, "%d", z);
	oled_putString(50, 32, sens_value_accZ, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	sprintf(sens_value_temp, "%.1f", temp);
	oled_putString(50, 42, sens_value_temp, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

}

boolean displaySetting() {

	static boolean toDisplaySwitch = false;

	if (btnSW4 == 0) {
		if (toDisplaySwitch == false) {
			toDisplaySwitch = true;
		} else (toDisplaySwitch == true) {
			toDisplaySwitch = false;
		}
	}
	return toDisplaySwitch;

}

void toFinish() {

	LPC_GPIOINT -> IO2IntEnF |= 1<<10;
	LPC_GPIOINT -> IO2IntClr |= 1<<5;
	LPC_GPIOINT -> IO2IntEnF |= 1<<5;
	light_setRange(LIGHT_RANGE_4000);
	light_setLoThreshold(Light_Low_Warning);

	NVIC_EnableIRQ(EINT3_IRQn);

	/*	//test sending message
		msg = "Welcome to EE2024 \r\n";
		UART_Send(LPC_UART3, (uint8_t *)msg , strlen(msg), BLOCKING);*/
	/*	//test receiving a letter and sending back to port
		UART_Receive(LPC_UART3, &data, 1, BLOCKING);
		UART_Send(LPC_UART3, &data, 1, BLOCKING);
		//test receiving message without knowing message length
		len = 0;*/

	/*	do
		{
			UART_Receive(LPC_UART3, &data, 1, BLOCKING);

			if (data != '\r')
			{
				len++;
				line[len-1] = data;
			}
		} while ((len<64) && (data != '\r'));*/
	/*	line[len]=0;
		UART_SendString(LPC_UART3, &line);
		printf("--%s--\n", line);*/

}

void monitorMode(boolean displaySetting) {

	initialiseSystem();
	while (1) {

		debounceTime = ticks / 1000;
		// If interrupts come faster than 100ms, assume it's a bounce and ignore
		if (debounceTime - lastDebounceTime > debounceDelay) {
			lastDebounceTime = debounceTime;
			btn1 = ((GPIO_ReadValue(1) >> 31) & 0x01);
		}

		if (sTicks == UPDATE_INTERVAL_FIRST || sTicks == UPDATE_INTERVAL_SECOND ||
				sTicks == UPDATE_INTERVAL_THIRD) {
			cuteSensor_start();
			updateOledDisplay();
		}

		int accX_difference = x*x - xold*xold;
		int accY_difference = y*y - yold*yold;
		int accZ_difference = z*z - zold*zold;
		GPIO_ClearValue( 0, (1<<26));

		if ( (accX_difference > 1000 ||
				accX_difference < -1000 ||
				accY_difference > 1000 ||
				accY_difference < -1000 ||
				accZ_difference > 1000 ||
				accZ_difference < -1000) &&
				light < 300) {
			if(ticks%333000 < 166500) {
				GPIO_SetValue( 0, (1<<26) ); //blue
			} else {
				GPIO_ClearValue( 0, (1<<26) ); //blue
			}
		}

		if (temp > 45) {
			if(ticks%333000 < 166500) {
				GPIO_SetValue( 2, (1<<0) ); //red
			} else {
				GPIO_ClearValue( 2, (1<<0) ); //red
			}
		}


		if (songFlag == 1 && btn1 == 1){
			playSong("A100.");
		}

		Timer0_Wait(1);
	}

}

void stableMode(boolean displaySetting) {

	while (1) {
		if (displaySetting != displaySetting()) {
			break;
		}
	}

}

int main (void) {

	boolean displaySetting = displaySetting();

	while (1) {
		if (displaySetting == true) {
			monitorMode(displaySetting);
		} else {
			stableMode(displaySetting);
		}
	}

}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}

