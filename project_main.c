/* C Standard library */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <math.h>
#include <string.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/i2c/I2CCC26XX.h>

#include "buzzer.h"

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"

#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char uartTaskStackWrite[STACKSIZE];
Char taskStack[STACKSIZE];

enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;
enum sensorReadState { MENU, READGYRO, READLIGHT };
enum sensorReadState sensorState = MENU;


//Voi tehä järkevämmin Mr SpagettiCoder Illikainen
enum menuState {IDLE,FUN,SERIOUS};
enum menuState menuStatus = IDLE;


static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle hMpuPin;
static PIN_Handle powerButtonHandle;
static PIN_State powerButtonState;

const int max_morsecode_length = 5;
char morsecode_to_letter[36];
char codes[36][5] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};
const int code_length = 16;

static char char_to_send;

static uint16_t code = 0;
static int16_t position = 0;

float ambientLight = -1000.0;

//Buzzer
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

// Light Button / Send " " Button
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    //uint_t led_value = PIN_getInputValue(Board_LED0);
    //PIN_setOutputValue(ledHandle, Board_LED0, !led_value);
    PIN_setOutputValue(ledHandle, Board_LED0, 1);
    System_printf("buttonfxn");
    System_flush();
    char_to_send = ' ';
    programState = DATA_READY;
    Task_sleep(10000 / Clock_tickPeriod);
    PIN_setOutputValue(ledHandle, Board_LED0, 0);
}

//Power Button
PIN_Config powerButtonConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config powerButtonWakeConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};
void powerFxn(PIN_Handle handle, PIN_Id pinId) {
   Task_sleep(100000 / Clock_tickPeriod);

   PIN_close(powerButtonHandle);
   PINCC26XX_setWakeup(powerButtonWakeConfig);
   Power_shutdown(NULL,0);
}

// I2C config for reading gyro and accelerometer (should we have another const for other configs?)
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

void init_char_encoding(void) {
    code = 0;
    position = 14;
}

void morse_led(char letter) {
    const int point_period = 40000;
    switch (letter) {
        case ' ':
            PIN_setOutputValue(ledHandle, Board_LED0, 0);
            Task_sleep(point_period * 3 / Clock_tickPeriod);
            break;
        case '.':   
            PIN_setOutputValue(ledHandle, Board_LED0, 1);
            Task_sleep(point_period / Clock_tickPeriod);
            break;
        case '-':
            PIN_setOutputValue(ledHandle, Board_LED0, 1);
            Task_sleep(point_period * 3 / Clock_tickPeriod);
            break;
        default:
            return;
    }
    PIN_setOutputValue(ledHandle, Board_LED0, 0);
    Task_sleep(point_period / Clock_tickPeriod);
}

int encode_char_to_code(char* morse) {
    static char endchars[] = " \r\n";
    static int endpos = 0;
    
    if(*morse == endchars[endpos]) {
        if(endpos++ == 2) {
            endpos = 0;
            morse_led(' ');
            return 1;
        }
    } else {
        endpos = 0;
    }

    if(position < 0) 
        return 0;
    if (*morse != '.' || *morse != '-')
        return 0;
    code += (*morse - 44) << position;
    position -= 2;
    return 0;
}

int get_code(void) {
    return code;
}

void init_morsecode_converison(void) {
    int i;
    for (i = 0; i < 36; i++) {
        init_char_encoding();
        int j;
        for(j = 0; !encode_char_to_code(&codes[i][j]); j++);
        morsecode_to_letter[i] = get_code();
    } 
}

char decode_morse(uint16_t morse_code) {
    int i;
    for (i = 0; i < 36; i++) {
        if (morsecode_to_letter[i] == morse_code) {
            if(i < 26) {
                return i + 'a';
            } else {
                return i + 22;
            }
        }
    }
    return '?';
}

int send_char(UART_Handle uart, char letter) {
    char sendable[] = "c\r\n";
    sendable[0] = letter;
    return UART_write(uart, &sendable, 4); // also sends null
}

char readUART(UART_Handle uart) {
    init_char_encoding();
    char char_received;
    do {
        UART_read(uart, &char_received, 1);
        char received_msg[100];
        sprintf(received_msg, "Received %x, which is %c\n", char_received, char_received);
        System_printf(received_msg);
        System_flush();
        morse_led(char_received);
    } while(!encode_char_to_code(&char_received));
    return decode_morse(get_code());
}

//UART TASK

char UARTBuffer[1];
#define BUFFER_SIZE 256

typedef struct {
    uint8_t data[BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

RingBuffer uartBuffer = { .head = 0, .tail = 0 };

void RingBuffer_Write(RingBuffer *buffer, uint8_t byte) {
    uint16_t next = (buffer->head + 1) % BUFFER_SIZE;

    if (next != buffer->tail) {
        buffer->data[buffer->head] = byte;
        buffer->head = next;
    }
}

int RingBuffer_Read(RingBuffer *buffer, uint8_t *byte) {
    if (buffer->head == buffer->tail) {
        return -1;
    }
    *byte = buffer->data[buffer->tail];
    buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
    return 0;
}

void uartReadCallback(UART_Handle uart, void *buffer, size_t count) {
    char receivedChar = *((char *)buffer);

    RingBuffer_Write(&uartBuffer, (uint8_t)receivedChar);

    UART_read(uart, UARTBuffer, 1);
}

Void uartTaskFxnRead(UArg arg0, UArg arg1) {

    UART_Handle uart;
    UART_Params uartParams;

    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.baudRate = 9600;
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;
    uartParams.readCallback = uartReadCallback;

    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
       System_abort("Error opening the UART read");
    }

    UART_read(uart, UARTBuffer, 1);

    while (1) {
        uint8_t byte;
        while (RingBuffer_Read(&uartBuffer, &byte) == 0) {
            morse_led(byte);

            char message[100];
            sprintf(message, "Processed %c", byte);
            System_printf("%s\n", message);
            System_flush();
        }

        if (programState == DATA_READY) {
           send_char(uart, char_to_send);
           programState = WAITING;
       }

       Task_sleep(100000 / Clock_tickPeriod);
    }
}

//SENSOR TASK

Void sensorTaskFxn(UArg arg0, UArg arg1) {
    I2C_Handle i2c;
    I2C_Params i2cParams;

    switch (sensorState) {
        case READGYRO: case MENU:
            I2C_Params_init(&i2cParams);
            i2cParams.bitRate = I2C_400kHz;
            i2cParams.custom = (uintptr_t)&i2cMPUCfg;
            // Power the MPU9250 sensor. Must do I2C_Close() before using another sensor, and using I2C_open() again.
            PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

            System_printf("MPU9250: Power ON\n");
            System_flush();

            i2c = I2C_open(Board_I2C, &i2cParams);
            if (i2c == NULL) {
                System_abort("Error Initializing I2CMPU\n");
            }

            System_printf("MPU9250: Setup and calibration...\n");
            System_flush();
            Task_sleep(100000 / Clock_tickPeriod);
            mpu9250_setup(&i2c);
            System_printf("MPU9250: Setup and calibration OK\n");
            System_flush();
            break;
        case READLIGHT:

            I2C_Params_init(&i2cParams);
            i2cParams.bitRate = I2C_400kHz;
            
            // Power the OPT3001 sensor. Must do I2C_Close() before using another sensor, and using I2C_open() again.
            System_printf("OPT3001: Power ON\n");
            System_flush();

            i2c = I2C_open(Board_I2C_TMP, &i2cParams);
            if (i2c == NULL) {
            System_abort("Error Initializing I2C\n");
            }

            System_printf("OPT3001: Setup and calibration...\n");
            System_flush();
            Task_sleep(100000 / Clock_tickPeriod);
            opt3001_setup(&i2c);
            System_printf("OPT3001: Setup and calibration OK\n");
            System_flush();
            break;
        default:
            break;
    }
    double previous_time =  Clock_getTicks()/(10000 / Clock_tickPeriod); // tick period is us 1000000 us in second
    float rotation_x = 0.0;
    bool rotated_90 = false;
    float rotation_z = 0.0;
    bool rotated_90_z = false;
    double previousTime = Clock_getTicks()/(100000 / Clock_tickPeriod);
 
    //Menu related variables
    const double menuMovementThreshold = 0.40;  //TODO: Move consts to the top of file
    const double timerLimit = 5000; // in deciseconds
    double timer = 0;
    char debug_msg[100];
    while (1) {
        switch (sensorState){
            // Should programState be rad and if it is DATA_READY it wouldn't be read again
            case MENU: {
                PIN_setOutputValue(ledHandle, Board_LED0, 1);
                float ax, ay, az, gx, gy, gz;
                mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);
                sprintf(debug_msg,"ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f\n",ax, ay, az, gx, gy, gz);
                System_printf(debug_msg);
                System_flush();

                if( menuStatus == IDLE && menuMovementThreshold < ax){
                    buzzerOpen(hBuzzer);
                    buzzerSetFrequency(2000);
                    Task_sleep(500000 / Clock_tickPeriod);
                    buzzerClose();
                    menuStatus = SERIOUS;
                }
                else if(menuStatus == IDLE && ax < -menuMovementThreshold){
                    //menuStatus = FUN;
                }
                else if(menuStatus != IDLE){
                    double deltaTime = Clock_getTicks()/(10000 / Clock_tickPeriod) - previous_time;
                    previous_time = Clock_getTicks()/(10000 / Clock_tickPeriod);
                    timer += deltaTime;
                    sprintf(debug_msg,"Delta time: %f timer: %f", deltaTime, timer);
                    System_printf(debug_msg);
                    System_flush();

                    if(menuStatus == SERIOUS && timer <= timerLimit &&  menuMovementThreshold < ay ){
                        PIN_setOutputValue(ledHandle, Board_LED0, 0);
                        sensorState = READGYRO;
                        menuStatus = IDLE;
                        timer = 0;
                    }
                    else if(menuStatus == FUN && timer <= timerLimit && menuMovementThreshold < ay ){
                        //sensorState play aduio
                        menuStatus = IDLE;
                        timer = 0;
                    }
                    else if(timerLimit < timer ){
                        buzzerOpen(hBuzzer);
                        buzzerSetFrequency(2000);
                        Task_sleep(500000 / Clock_tickPeriod);
                        buzzerClose();
                        buzzerOpen(hBuzzer);
                        buzzerSetFrequency(2000);
                        Task_sleep(500000 / Clock_tickPeriod);
                        buzzerClose();
                        menuStatus = IDLE;
                        timer = 0;
                    }
                }
                else {
                    timer = 0;
                }


                break;
            }
            case READGYRO: {
                float threshold = 0.4f;
                float threshold_z = 1.3f;
                float ax, ay, az, gx, gy, gz;
                mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);
                if (ax > 0.9f && ay < 0.1f && az < 0.1f && gx < 1.0f && gy < 1.0f && gz < 1.0f){
                    buzzerOpen(hBuzzer);
                    buzzerSetFrequency(2000);
                    Task_sleep(100000 / Clock_tickPeriod);
                    buzzerClose();

                    sensorState = MENU;
                }
                double time = Clock_getTicks()/(10000 / Clock_tickPeriod);

                sprintf(debug_msg,"ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f\n",ax, ay, az, gx, gy, gz);
                System_printf(debug_msg);
                System_flush();

                if(ax > threshold && ay < threshold && !rotated_90 && !rotated_90_z && az < 1.1f){
                    buzzerOpen(hBuzzer);
                    buzzerSetFrequency(2000);
                    Task_sleep(100000 / Clock_tickPeriod);
                    buzzerClose();
                    rotated_90 = true;
                    char_to_send = '.';
                    programState = DATA_READY;
                }

                if(az > threshold_z && ax < threshold && !rotated_90_z && !rotated_90){
                    buzzerOpen(hBuzzer);
                    buzzerSetFrequency(2000);
                    Task_sleep(500000 / Clock_tickPeriod);
                    buzzerClose();
                    rotated_90_z = true;
                    char_to_send = '-';
                    programState = DATA_READY;
                }

                if(rotated_90_z && az < threshold_z){
                    rotated_90_z = false;
                }

                if(rotated_90 && ax < threshold){
                    rotated_90 = false;
                }

                break;
            }
            case READLIGHT: {
                double data = opt3001_get_data(&i2c);
                char debug_msg[100];
                sprintf(debug_msg,"...%f",data);
                System_printf(debug_msg);
                System_flush();

                ambientLight = data;
                programState = DATA_READY;

                //Change sensorState and close connection.
                //I2C_close();
                break;
            }
            default: {
                System_printf("Running default case. Not reading any sensors.\n");
                System_flush();
                break;
            }
        }
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

//BUZZER TASK

Void taskFxn(UArg arg0, UArg arg1) {

  while (1) {
    buzzerOpen(hBuzzer);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();

    Task_sleep(950000 / Clock_tickPeriod);
  }

}


//MAIN PROGRAM AND INITS

Int main(void) {
    //SensorTask
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;

    //Uart
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    //Buzzer
    Task_Handle task;
    Task_Params taskParams;

    //Inits
    Board_initGeneral();
    Board_initI2C();
    Board_initUART();

    // Button and led inits
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
       System_abort("Error initializing button pins\n");
    }
    ledHandle = PIN_open(&ledState, ledConfig);
    if(!ledHandle) {
       System_abort("Error initializing LED pins\n");
    }
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }
    /*powerButtonHandle = PIN_open(&powerButtonState, powerButtonConfig);
    if(!powerButtonHandle) {
        System_abort("Error initializing power button\n");
    }
    if (PIN_registerIntCb(powerButtonHandle, &powerFxn) != 0) {
        System_abort("Error registering power button callback");
    }*/

    //Task Inits
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxnRead, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /*hBuzzer = PIN_open(&sBuzzer, cBuzzer);
    if (hBuzzer == NULL) {
        System_abort("Pin open failed!");
    }

    Task_Params_init(&taskParams);
    taskParams.stackSize = STACKSIZE;
    taskParams.stack = &taskStack;
    task = Task_create((Task_FuncPtr)taskFxn, &taskParams, NULL);
    if (task == NULL) {
        System_abort("Task create failed!");
    }*/

    //Sanity Check
    System_printf("Hello world!\n");
    System_flush();

    BIOS_start();

    return (0);
}
