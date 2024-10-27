/* C Standard library */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

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
enum sensorReadState { READGYRO, READLIGHT };
enum sensorReadState sensorState = READGYRO;

static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;
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
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
// I2C config for reading gyro and accelerometer (should we have another const for other configs?)
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

// Power Button
PIN_Config powerButtonConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config powerButtonWakeConfig[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};
Void powerFxn(PIN_Handle handle, PIN_Id pinId) {
   Task_sleep(100000 / Clock_tickPeriod);

   PIN_close(powerButtonHandle);
   PINCC26XX_setWakeup(powerButtonWakeConfig);
   Power_shutdown(NULL,0);
}

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    uint_t pinValue = PIN_getOutputValue(Board_LED0);
    pinValue = !pinValue;
    PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {

    UART_Handle uart;
    UART_Params uartParams;

    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode=UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;

    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
       System_abort("Error opening the UART");
    }

    while (1) {

        // if(programState == DATA_READY){
        //     char debug_msg[100];
        //     sprintf(debug_msg,"...%f",ambientLight);
        //     System_printf(debug_msg);
        //     System_flush();
        //     char echo_msg[100];
        //     UART_write(uart, debug_msg, strlen(debug_msg));
        //     programState = WAITING;
        // }

        System_printf("taalla");
        System_flush();
        char char_received;
        char debug_msg[100];
        UART_read(uart, &char_received, 1);     
        sprintf(debug_msg, "Received: %c",char_received);
        System_printf(debug_msg);
        System_flush();

        programState = WAITING;
        
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {
    switch (sensorState) {
        case READGYRO:
            I2C_Handle i2cMPU;
            I2C_Params i2cMPUParams;

            I2C_Params_init(&i2cMPUParams);
            i2cMPUParams.bitRate = I2C_400kHz;
            i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
            // Power the MPU9250 sensor. Must do I2C_Close() before using another sensor, and using I2C_open() again.
            PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

            System_printf("MPU9250: Power ON\n");
            System_flush();

            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
            if (i2cMPU == NULL) {
                System_abort("Error Initializing I2CMPU\n");
            }

            System_printf("MPU9250: Setup and calibration...\n");
            System_flush();
            Task_sleep(100000 / Clock_tickPeriod);
            mpu9250_setup(&i2cMPU);
            System_printf("MPU9250: Setup and calibration OK\n");
            System_flush();
            break;
        case READLIGHT:
            I2C_Handle i2c;
            I2C_Params i2cParams;
            I2C_Transaction i2cMessage;

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

    while (1) {
        switch (sensorState){
            case READGYRO:
                float ax, ay, az, gx, gy, gz;
                float rotation_x = 0.0;
                bool rotated_90 = false;

                mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

                char debug_msg[100];
                sprintf(debug_msg,"ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f\n",ax, ay, az, gx, gy, gz);
                System_printf(debug_msg);
                System_flush();

                if (fabs(gx) > 20.0) {
                    rotation_x += gx * (0.01);
                } else {
                    rotation_x *= 0.9;
                }

                //Detect 90 degrees rotation and 90 degrees rotation back for X-axis
                if (!rotated_90 && fabs(rotation_x) >= 90.0) {
                    System_printf("90-degree rotation detected on X-axis!\n");
                    System_flush();
                    rotated_90 = true;
                }
                if (rotated_90 && fabs(rotation_x) < 10.0) {
                    System_printf("Returned to starting position on X-axis.\n");
                    System_flush();
                    rotated_90 = false;
                    rotation_x = 0.0;
                }

                programState = DATA_READY;

                Task_sleep(10000 / Clock_tickPeriod);

                //Change sensorState and close connection.
                //I2C_close();

                break;
            case READLIGHT:
                double data = opt3001_get_data(&i2c);

                char debug_msg[100];
                sprintf(debug_msg,"...%f",data);
                System_printf(debug_msg);
                System_flush();

                ambientLight = data;
                programState = DATA_READY;

                Task_sleep(100000 / Clock_tickPeriod);

                //Change sensorState and close connection.
                //I2C_close();
                break;
            default:
                System_printf("Running default case. Not reading any sensors.\n");
                System_flush();
                Task_sleep(10000 / Clock_tickPeriod);
                break;
        }
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
