/*
 * FreeRTOS STM32 Reference Integration
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 */

/* Standard includes. */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Utils includes. */
#include "FreeRTOS_CLI.h"

#include "cli.h"

static void prvPSCommand( ConsoleIO_t * const pxConsoleIO,
                          uint32_t ulArgc,
                          char * ppcArgv[] );

static void vKillAllCommand( ConsoleIO_t * const pxCIO,
                             uint32_t ulArgc,
                             char * ppcArgv[] );

static void vKillCommand( ConsoleIO_t * const pxCIO,
                          uint32_t ulArgc,
                          char * ppcArgv[] );


const CLI_Command_Definition_t xCommandDef_ps =
{
    "ps",
    "ps\r\n"
    "    List the status of all running tasks and related runtime statistics.\r\n\n",
    prvPSCommand
};

const CLI_Command_Definition_t xCommandDef_kill =
{
    "kill",
    "kill\r\n"
    "    kill [ -SIGNAME ] <Task ID>\r\n"
    "    Signal a task with the named signal and the specified task id.\r\n\n"
    "    kill [ -n ] <Task ID>\r\n"
    "    Signal a task with the given signal number and the specified task id.\r\n\n",
    vKillCommand
};


const CLI_Command_Definition_t xCommandDef_killAll =
{
    "killall",
    "    killall [ -SIGNAME ] <Task Name>\r\n"
    "    killall [ -n ] <Task Name>\r\n"
    "        Signal a task with a given name with the signal number or name given.\r\n\n",
    vKillAllCommand
};



/*-----------------------------------------------------------*/

/* Returns up to 9 character string representing the task state */
static inline const char * pceTaskStateToString( eTaskState xState )
{
    const char * pcState;
    switch( xState )
    {
    case eRunning:
        pcState = "RUNNING";
        break;
    case eReady:
        pcState = "READY";
        break;
    case eBlocked:
        pcState = "BLOCKED";
        break;
    case eSuspended:
        pcState = "SUSPENDED";
        break;
    case eDeleted:
        pcState = "DELETED";
        break;
    case eInvalid:
    default:
        pcState = "UNKNOWN";
        break;
    }
    return pcState;
}

/*-----------------------------------------------------------*/

static uint32_t ulGetStackDepth( TaskHandle_t xTask )
{
    struct tskTaskControlBlock
    {
        volatile StackType_t * pxDontCare0;

        #if ( portUSING_MPU_WRAPPERS == 1 )
            xMPU_SETTINGS xDontCare1;
        #endif
        ListItem_t xDontCare2;
        ListItem_t xDontCare3;
        UBaseType_t uxDontCare4;
        StackType_t * pxStack;
        char pcDontCare5[ configMAX_TASK_NAME_LEN ];

        #if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
            StackType_t * pxEndOfStack;
        #endif
    };
    struct tskTaskControlBlock * pxTCB = ( struct tskTaskControlBlock * ) xTask;

    return( ( ( ( uintptr_t ) pxTCB->pxEndOfStack - ( uintptr_t ) pxTCB->pxStack ) / sizeof( StackType_t ) ) + 2 );
}

static void prvPSCommand( ConsoleIO_t * const pxCIO,
                          uint32_t ulArgc,
                          char * ppcArgv[] )
{
    UBaseType_t uxNumTasks = uxTaskGetNumberOfTasks();

    TaskStatus_t * pxTaskStatusArray = ( TaskStatus_t * ) pvPortMalloc( sizeof( TaskStatus_t ) * uxNumTasks );

    if( pxTaskStatusArray == NULL )
    {
        pxCIO->print( "Error: Not enough memory to complete the operation" );
    }
    else
    {
        unsigned long ulTotalRuntime = 0;
        uxNumTasks = uxTaskGetSystemState( pxTaskStatusArray,
                                           uxNumTasks,
                                           &ulTotalRuntime );

        ulTotalRuntime /= 100;

        snprintf( pcCliScratchBuffer, CLI_OUTPUT_SCRATCH_BUF_LEN, "Total Runtime: %lu\r\n", ulTotalRuntime );

        pxCIO->print( pcCliScratchBuffer );

        pxCIO->print( "+----------------------------------------------------------------------------------+\r\n" );
        pxCIO->print( "| Task |   State   |    Task Name     |___Priority__| %CPU | Stack | Stack | Stack |\r\n" );
        pxCIO->print( "|  ID  |           |                  | Base | Cur. |      | Alloc |  HWM  | Usage |\r\n" );
        pxCIO->print( "+----------------------------------------------------------------------------------+\r\n" );
                   /* "| 1234 | AAAAAAAAA | AAAAAAAAAAAAAAAA |  00  |  00  | 000% | 00000 | 00000 | 000%  |" */

        for( uint32_t i = 0; i < uxNumTasks; i++ )
        {
            uint32_t ulStackSize = ulGetStackDepth( pxTaskStatusArray[ i ].xHandle );
            uint32_t ucStackUsagePct = ( 100 * ( ulStackSize - pxTaskStatusArray[ i ].usStackHighWaterMark ) / ulStackSize );
            snprintf( pcCliScratchBuffer, CLI_OUTPUT_SCRATCH_BUF_LEN,
                      "| %4lu | %-9s | %-16s |  %2lu  |  %2lu  | %3lu%% | %5lu | %5lu | %3lu%%  |\r\n",
                      pxTaskStatusArray[ i ].xTaskNumber,
                      pceTaskStateToString( pxTaskStatusArray[ i ].eCurrentState ),
                      pxTaskStatusArray[ i ].pcTaskName,
                      pxTaskStatusArray[ i ].uxBasePriority,
                      pxTaskStatusArray[ i ].uxCurrentPriority,
                      pxTaskStatusArray[ i ].ulRunTimeCounter / ulTotalRuntime,
                      ulStackSize,
                      ( uint32_t ) pxTaskStatusArray[ i ].usStackHighWaterMark,
                      ucStackUsagePct ) ;

            pxCIO->print( pcCliScratchBuffer );
        }

        vPortFree( pxTaskStatusArray );
    }
}

typedef enum
{
    SIGHUP = 1,
    SIGINT = 2,
    SIGQUIT = 3,
    SIGKILL = 9,
    SIGTERM = 15,
    SIGSTOP = 23,
    SIGSTP = 24,
    SIGCONT = 25
} Signal_t;

struct
{
    Signal_t xSignal;
    const char * const pcSignalName;
}
pcSignaMap [] =
{
    { SIGHUP, "SIGHUP" },
    { SIGINT, "SIGINT"},
    { SIGQUIT, "SIGQUIT" },
    { SIGKILL, "SIGKILL" },
    { SIGTERM, "SIGTERM" },
    { SIGSTOP, "SIGSTOP" },
    { SIGSTP, "SIGSTP" },
    { SIGCONT, "SIGCONT" }
};

static void vSignalTask( TaskHandle_t xTask, Signal_t xSignal )
{
    switch( xSignal )
    {
    case SIGQUIT:
    case SIGTERM:
        vTaskSuspend( xTask );
        vTaskDelete( xTask );
        break;
    case SIGSTOP:
    case SIGSTP:
        vTaskSuspend( xTask );
        break;
    case SIGCONT:
        vTaskResume( xTask );
        break;
    default:
        break;
    }
}

/* This is a really dumb way to do this */
static TaskHandle_t xGetTaskHandleFromID( UBaseType_t uxTaskID )
{
    TaskHandle_t xTaskHandle = NULL;
    UBaseType_t uxNumTasks = uxTaskGetNumberOfTasks();

    TaskStatus_t * pxTaskStatusArray = ( TaskStatus_t * ) pvPortMalloc( sizeof( TaskStatus_t ) * uxNumTasks );

    if( pxTaskStatusArray != NULL )
    {
        unsigned long ulTotalRuntime = 0;
        uxNumTasks = uxTaskGetSystemState( pxTaskStatusArray,
                                           uxNumTasks,
                                           &ulTotalRuntime );

        for( uint32_t i = 0; i < uxNumTasks; i++ )
        {

            if( pxTaskStatusArray[ i ].xTaskNumber == uxTaskID )
            {
                xTaskHandle = pxTaskStatusArray[ i ].xHandle;
                break;
            }
        }
        vPortFree( pxTaskStatusArray );
    }

    return xTaskHandle;
}

static void vKillCommand( ConsoleIO_t * const pxCIO,
                          uint32_t ulArgc,
                          char * ppcArgv[] )
{
    Signal_t xTargetSignal = SIGTERM;
    UBaseType_t uxTaskId = 0;

    for( uint32_t i = 0; i < ulArgc; i++ )
    {
        /* Arg is a signal number or name */
        if( ppcArgv[ i ][ 0 ] == '-' )
        {
            char * pcArg = &( ppcArgv[ i ][ 1 ] );
            uint32_t ulSignal = strtoul( pcArg, NULL, 10 );
            if( ulSignal != 0 )
            {
                xTargetSignal = ulSignal;
            }
            else
            {
                for(uint32_t i = 0; i < ( sizeof( pcSignaMap ) / sizeof( pcSignaMap[ 0 ] ) ); i++ )
                {
                    if( strcmp( pcSignaMap[ i ].pcSignalName, pcArg ) == 0 )
                    {
                        xTargetSignal = pcSignaMap[ i ].xSignal;
                        break;
                    }
                }
            }
        }
        else /* Arg is a task number */
        {
            uxTaskId = strtoul( ppcArgv[ i ], NULL, 10 );
        }

        if( uxTaskId != 0 )
        {
            vSignalTask( xGetTaskHandleFromID( uxTaskId ), xTargetSignal );
        }
    }
}

static void vKillAllCommand( ConsoleIO_t * const pxCIO,
                             uint32_t ulArgc,
                             char * ppcArgv[] )
{
    Signal_t xTargetSignal = SIGTERM;
    TaskHandle_t xTaskHandle = NULL;

    for( uint32_t i = 0; i < ulArgc; i++ )
    {
        /* Arg is a signal number or name */
        if( ppcArgv[ i ][ 0 ] == '-' )
        {
            char * pcArg = &( ppcArgv[ i ][ 1 ] );
            uint32_t ulSignal = strtoul( pcArg, NULL, 10 );
            if( ulSignal != 0 )
            {
                xTargetSignal = ulSignal;
            }
            else
            {
                for(uint32_t i = 0; i < ( sizeof( pcSignaMap ) / sizeof( pcSignaMap[ 0 ] ) ); i++ )
                {
                    if( strcmp( pcSignaMap[ i ].pcSignalName, pcArg ) == 0 )
                    {
                        xTargetSignal = pcSignaMap[ i ].xSignal;
                        break;
                    }
                }
            }
        }
        else
        {
            xTaskHandle = xTaskGetHandle( ppcArgv[ i ] );
        }

        if( xTaskHandle != NULL )
        {
            vSignalTask( xTaskHandle, xTargetSignal );
        }
    }
}
