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

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"

#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

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

float ambientLight = -1000.0;

// Light Button
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};
// I2C config for reading gyro and accelerometer (should we have another const for other configs?)
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};
const int max_morsecode_length = 5;
char morsecode_to_letter[36];
char codes[36][5] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."}; 
const int code_length = 16;

static char char_to_send;

static uint16_t code = 0;
static int16_t position = 0;

void init_char_encoding(void) {
    code = 0;
    position = 14;
}

void morse_led(char letter) {
    const int point_period = 400000;
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

// Power Button
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

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    uint_t led_value = PIN_getInputValue(Board_LED0);
    PIN_setOutputValue(ledHandle, Board_LED0, !led_value);
    System_printf("buttonfxn");
    System_flush();
    char_to_send = ' ';
    programState = DATA_READY;
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

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {

    UART_Handle uart;
    UART_Params uartParams;

    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_ON;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;

    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
       System_abort("Error opening the UART");
    }
    //init_morsecode_converison();
    while (1) {
         if(programState == DATA_READY){
             send_char(uart, char_to_send);
             programState = WAITING;
         } else {
            char received[] = "123";
            UART_read(uart, &received, 3);
            //morse_led(received);
            char message[100];
            sprintf(message, "Hex %x %x %x char %s", received[0], received[1], received[2], received);
            System_printf(message);
            System_flush();
         }
        //UART_read(uart, &char_received, 1);
        //sprintf(debug_msg, "Received: %c",char_received);
        //UART_write(uart, &char_received, 4);
        
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

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
    double previous_time =  Clock_getTicks()/(100000 / Clock_tickPeriod); // tick period is us 1000000 us in second
    float rotation_x = 0.0;
    bool rotated_90 = false;
    float rotation_z = 0.0;
    bool rotated_90_z = false;
    double previousTime = Clock_getTicks()/(100000 / Clock_tickPeriod);
 
    //Menu related variables
    const double menuMovementThreshold = 0.50;  //TODO: Move consts to the top of file
    const double timerLimit = 100; // in deciseconds
    double timer = 0;
    char debug_msg[100];
    while (0) {
        switch (sensorState){
            // Should programState be rad and if it is DATA_READY it wouldn't be read again
            case MENU: {
                float ax, ay, az, gx, gy, gz;
                mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);
                sprintf(debug_msg,"ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f",ax, ay, az, gx, gy, gz);
                System_printf(debug_msg);
                System_flush();

                if( menuStatus == IDLE && menuMovementThreshold < ax){
                    menuStatus = SERIOUS;
                }
                else if(menuStatus == IDLE && ax < -menuMovementThreshold){
                    menuStatus = FUN;
                }
                else if(menuStatus != IDLE){
                    double deltaTime = Clock_getTicks()/(100000 / Clock_tickPeriod) - previous_time;
                    previous_time = Clock_getTicks()/(100000 / Clock_tickPeriod);
                    timer += deltaTime;
                    sprintf(debug_msg,"Delta time: %f timer: %f", deltaTime, timer);
                    System_printf(debug_msg);
                    System_flush();

                    if(menuStatus == SERIOUS && timer <= timerLimit &&  menuMovementThreshold < ay ){
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
                float ax, ay, az, gx, gy, gz;
                mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);
                if(ax < - 0.75 && ay < 0.1 && az < 0.1 && gx < 1 && gy < 1 && gz < 1){
                    sensorState = MENU;
                }

                double time = Clock_getTicks()/(100000 / Clock_tickPeriod); // tick period is us 1000000 us in second

                sprintf(debug_msg,"ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f",ax, ay, az, gx, gy, gz);
                System_printf(debug_msg);
                System_flush();
                sprintf(debug_msg, "rotation_x %f, rotation_z %f", rotation_x, rotation_z);
                System_printf(debug_msg);
                System_flush();

                if (fabs(gx) > 10.0) {
                    rotation_x += floor(gx) * (time - previous_time);
                } else {
                    rotation_x *= 0.9;
                } 

                //Detect 90 degrees rotation and 90 degrees rotation back for X-axis
                if (!rotated_90 && fabs(rotation_x) >= 90.0) {
                    System_printf("90-degree rotation detected on X-axis!\n");
                    System_flush();
                    rotated_90 = true;
                }
                if (rotated_90 && rotation_x < 10.0) {
                    System_printf("Returned to starting position on X-axis.\n");
                    System_flush();
                    rotated_90 = false;
                    rotation_x = 0.0;
                    char_to_send = '-';
                    programState = DATA_READY;
                }

                if (fabs(gz) > 10.0) {
                    rotation_z += floor(gz) * (time - previous_time);
                } else {
                    rotation_z *= 0.9;
                } 

                if (!rotated_90_z && fabs(rotation_z) >= 90.0) {
                    System_printf("90-degree rotation detected on Z-axis!\n");
                    System_flush();
                    rotated_90_z = true;
                }
                if (rotated_90_z && rotation_z < 10.0) {
                    System_printf("Returned to starting position on Z-axis.\n");
                    System_flush();
                    rotated_90_z = false;
                    rotation_z = 0.0;
                    char_to_send = '.';
                    programState = DATA_READY;
                }
                previous_time = time;
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
        Task_sleep(10000 / Clock_tickPeriod);
    }
}


Int main(void) {
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    Board_initGeneral();
    Board_initI2C();
    Board_initUART();

    // Led Init + Interruption init
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
    // Power button init
    powerButtonHandle = PIN_open(&powerButtonState, powerButtonConfig);
    if(!powerButtonHandle) {
        System_abort("Error initializing power button\n");
    }
    if (PIN_registerIntCb(powerButtonHandle, &powerFxn) != 0) {
        System_abort("Error registering power button callback");
    }

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
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    BIOS_start();

    return (0);
}
