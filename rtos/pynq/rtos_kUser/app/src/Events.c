/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.

    ***************************************************************************
    *                                                                         *
    * If you are:                                                             *
    *                                                                         *
    *    + New to FreeRTOS,                                                   *
    *    + Wanting to learn FreeRTOS or multitasking in general quickly       *
    *    + Looking for basic training,                                        *
    *    + Wanting to improve your FreeRTOS skills and productivity           *
    *                                                                         *
    * then take a look at the FreeRTOS eBook                                  *
    *                                                                         *
    *        "Using the FreeRTOS Real Time Kernel - a Practical Guide"        *
    *                  http://www.FreeRTOS.org/Documentation                  *
    *                                                                         *
    * A pdf reference manual is also available.  Both are usually delivered   *
    * to your inbox within 20 minutes to two hours when purchased between 8am *
    * and 8pm GMT (although please allow up to 24 hours in case of            *
    * exceptional circumstances).  Thank you for your support!                *
    *                                                                         *
    ***************************************************************************

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

/**
 * This file exercises the event mechanism whereby more than one task is
 * blocked waiting for the same event.
 *
 * The demo creates five tasks - four 'event' tasks, and a controlling task.
 * The event tasks have various different priorities and all block on reading
 * the same queue.  The controlling task writes data to the queue, then checks
 * to see which of the event tasks read the data from the queue.  The
 * controlling task has the lowest priority of all the tasks so is guaranteed
 * to always get preempted immediately upon writing to the queue.
 *
 * By selectively suspending and resuming the event tasks the controlling task
 * can check that the highest priority task that is blocked on the queue is the
 * task that reads the posted data from the queue.
 *
 * Two of the event tasks share the same priority.  When neither of these tasks
 * are suspended they should alternate - one reading one message from the queue,
 * the other the next message, etc.
 */

/* Standard includes. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo program include files. */
#include "mevents.h"
#include "Print.h"
#include "checkErrors.h"


/******************************** Definitions *********************************/
/* Demo specific constants. */
#define evtSTACK_SIZE		( ( unsigned portBASE_TYPE ) configMINIMAL_STACK_SIZE )
#define evtNUM_TASKS		( 4 )
#define evtQUEUE_LENGTH		( ( unsigned portBASE_TYPE ) 3 )
#define evtNO_DELAY						0

/* Just indexes used to uniquely identify the tasks.  Note that two tasks are
'highest' priority. */
#define evtHIGHEST_PRIORITY_INDEX_2		3
#define evtHIGHEST_PRIORITY_INDEX_1		2
#define evtMEDIUM_PRIORITY_INDEX		1
#define evtLOWEST_PRIORITY_INDEX		0

#define MAGIC_EVENT_VALUE	(498U)


/*********************************** Values ***********************************/

/* Each event task increments one of these counters each time it reads data
from the queue. */
static volatile portBASE_TYPE xTaskCounters[ evtNUM_TASKS ] = { 0, 0, 0, 0 };

/* Each time the controlling task posts onto the queue it increments the
expected count of the task that it expected to read the data from the queue
(i.e. the task with the highest priority that should be blocked on the queue).

xExpectedTaskCounters are incremented from the controlling task, and
xTaskCounters are incremented from the individual event tasks - therefore
comparing xTaskCounters to xExpectedTaskCounters shows whether or not the
correct task was unblocked by the post. */
static portBASE_TYPE xExpectedTaskCounters[ evtNUM_TASKS ] = { 0, 0, 0, 0 };

/* Handles to the four event tasks.  These are required to suspend and resume
the tasks. */
static xTaskHandle xCreatedTasks[ evtNUM_TASKS ];

/* The single queue onto which the controlling task posts, and the four event
tasks block. */
static xQueueHandle xQueue;

/* Flag used to indicate whether or not an error has occurred at any time.
An error is either the queue being full when not expected, or an unexpected
task reading data from the queue. */
static portBASE_TYPE xHealthStatus = pdPASS;


/********************************* Prototypes *********************************/

/* Function that implements the event task.  This is created four times. */
static void prvMultiEventTask( void *pvParameters );

/* Function that implements the controlling task. */
static void prvEventControllerTask( void *pvParameters );

/* This is a utility function that posts data to the queue, then compares
xExpectedTaskCounters with xTaskCounters to ensure everything worked as
expected.

The event tasks all have higher priorities the controlling task.  Therefore
the controlling task will always get preempted between writhing to the queue
and checking the task counters.

@param xExpectedTask  The index to the task that the controlling task thinks
                      should be the highest priority task waiting for data, and
					  therefore the task that will unblock.

@param	xIncrement    The number of items that should be written to the queue.
*/
static void prvCheckTaskCounters( portBASE_TYPE xExpectedTask, portBASE_TYPE xIncrement );

/* This is just incremented each cycle of the controlling tasks function so
the main application can ensure the test is still running. */
static portBASE_TYPE xCheckVariable = 0;
// last iteration count of the above value
static portBASE_TYPE xPreviousCheckVariable = 0;

/*-----------------------------------------------------------*/

void vStartMultiEventTasks( void ) {

	/* Create the queue to be used for all the communications. */
	xQueue = xQueueCreate(
		evtQUEUE_LENGTH,
		( unsigned portBASE_TYPE ) sizeof( unsigned portBASE_TYPE )
	);

	/* Start the controlling task.  This has the idle priority to ensure it is
	always preempted by the event tasks. */
	xTaskCreate(
		prvEventControllerTask,
		"EvntCTRL",
		evtSTACK_SIZE,
		NULL,
		tskIDLE_PRIORITY,
		NULL
	);

	/* Start the four event tasks.  Note that two have priority 3, one
	priority 2 and the other priority 1. */
	/* COAST NOTE: this has been changed from passing addresses of the
	 *  counter to instead passing the index into the counter array.
	 * Although in this version the functions aren't protected by COAST,
     *  we use the same parameter passing scheme to make fault injection
     *  results more fairly comparable. */
	xTaskCreate(
		prvMultiEventTask,
		"Event0",
		evtSTACK_SIZE,
		( void * ) 0,
		1,
		&( xCreatedTasks[ evtLOWEST_PRIORITY_INDEX ] )
	);
	xTaskCreate(
		prvMultiEventTask,
		"Event1",
		evtSTACK_SIZE,
		( void * ) 1,
		2,
		&( xCreatedTasks[ evtMEDIUM_PRIORITY_INDEX ] )
	);
	xTaskCreate(
		prvMultiEventTask,
		"Event2",
		evtSTACK_SIZE,
		( void * ) 2,
		3,
		&( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] )
	);
	xTaskCreate(
		prvMultiEventTask,
		"Event3",
		evtSTACK_SIZE,
		( void * ) 3,
		3,
		&( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_2 ] )
	);
}
/*-----------------------------------------------------------*/

void vEndEventTasks() {
	int i;
	for (i = 0; i < evtNUM_TASKS; i+=1) {
#ifdef VERBOSE_KILL_TASKS
		xil_printf("Deleting task %d (%s)\r\n", i, pcTaskGetName(xCreatedTasks[i]));
#endif
		vTaskDelete(xCreatedTasks[i]);
	}
}
/*-----------------------------------------------------------*/

static void prvMultiEventTask( void *pvParameters ) {

	portBASE_TYPE *pxCounter;
	unsigned portBASE_TYPE uxDummy;
	unsigned portBASE_TYPE uxExpected = MAGIC_EVENT_VALUE;
	const char * const pcTaskStartMsg = "Multi event task started.\r\n";

	/* The variable this task will increment is passed in as a parameter. */
	pxCounter = ( portBASE_TYPE * ) &xTaskCounters[(uint32_t) pvParameters];

	vPrintDisplayMessage( &pcTaskStartMsg );

	for( ;; )
	{
		/* Block on the queue. */
		if( xQueueReceive( xQueue, &uxDummy, portMAX_DELAY ) )
		{
			if( uxDummy != uxExpected )
			{
				/* This is not what we expected to receive so an error has
				occurred. */
				reportError();
			}
			else
			{
				/* We unblocked by reading the queue - so simply increment
				the counter specific to this task instance. */
				( *pxCounter )++;
			}
		}
		else
		{
			xHealthStatus = pdFAIL;
			reportError();
		}
	}
}
/*-----------------------------------------------------------*/

static void prvEventControllerTask( void *pvParameters ) {

	const char * const pcTaskStartMsg = "Multi event controller task started.\r\n";
	portBASE_TYPE xDummy = 0;
	static portBASE_TYPE xSendVal = MAGIC_EVENT_VALUE;

	/* Just to stop warnings. */
	( void ) pvParameters;

	vPrintDisplayMessage( &pcTaskStartMsg );

	for( ;; )
	{
		/* All tasks are blocked on the queue.  When a message is posted one of
		the two tasks that share the highest priority should unblock to read
		the queue.  The next message written should unblock the other task with
		the same high priority, and so on in order.   No other task should
		unblock to read data as they have lower priorities. */

		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_1, 1 );
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_2, 1 );
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_1, 1 );
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_2, 1 );
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_1, 1 );

		/* For the rest of these tests we don't need the second 'highest'
		priority task - so it is suspended. */
		vTaskSuspend( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_2 ] );


		/* Now suspend the other highest priority task.  The medium priority
		task will then be the task with the highest priority that remains
		blocked on the queue. */
		vTaskSuspend( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );

		/* This time, when we post onto the queue we will expect the medium
		priority task to unblock and preempt us. */
		prvCheckTaskCounters( evtMEDIUM_PRIORITY_INDEX, 1 );

		/* Now try resuming the highest priority task while the scheduler is
		suspended.  The task should start executing as soon as the scheduler
		is resumed - therefore when we post to the queue again, the highest
		priority task should again preempt us. */
		vTaskSuspendAll();
			vTaskResume( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );
		xTaskResumeAll();
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_1, 1 );

		/* Now we are going to suspend the high and medium priority tasks.  The
		low priority task should then preempt us.  Again the task suspension is
		done with the whole scheduler suspended just for test purposes. */
		vTaskSuspendAll();
			vTaskSuspend( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );
			vTaskSuspend( xCreatedTasks[ evtMEDIUM_PRIORITY_INDEX ] );
		xTaskResumeAll();
		prvCheckTaskCounters( evtLOWEST_PRIORITY_INDEX, 1 );

		/* Do the same basic test another few times - selectively suspending
		and resuming tasks and each time calling prvCheckTaskCounters() passing
		to the function the number of the task we expected to be unblocked by
		the	post. */

		vTaskResume( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_1, 1 );

		vTaskSuspendAll(); /* Just for test. */
			vTaskSuspendAll(); /* Just for test. */
				vTaskSuspendAll(); /* Just for even more test. */
					vTaskSuspend( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );
				xTaskResumeAll();
			xTaskResumeAll();
		xTaskResumeAll();
		prvCheckTaskCounters( evtLOWEST_PRIORITY_INDEX, 1 );

		vTaskResume( xCreatedTasks[ evtMEDIUM_PRIORITY_INDEX ] );
		prvCheckTaskCounters( evtMEDIUM_PRIORITY_INDEX, 1 );

		vTaskResume( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );
		prvCheckTaskCounters( evtHIGHEST_PRIORITY_INDEX_1, 1 );

		/* Now a slight change, first suspend all tasks. */
		vTaskSuspend( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );
		vTaskSuspend( xCreatedTasks[ evtMEDIUM_PRIORITY_INDEX ] );
		vTaskSuspend( xCreatedTasks[ evtLOWEST_PRIORITY_INDEX ] );

		/* Now when we resume the low priority task and write to the queue 3
		times.  We expect the low priority task to service the queue three
		times. */
		vTaskResume( xCreatedTasks[ evtLOWEST_PRIORITY_INDEX ] );
		prvCheckTaskCounters( evtLOWEST_PRIORITY_INDEX, evtQUEUE_LENGTH );

		/* Again suspend all tasks (only the low priority task is not suspended
		already). */
		vTaskSuspend( xCreatedTasks[ evtLOWEST_PRIORITY_INDEX ] );

		/* This time we are going to suspend the scheduler, resume the low
		priority task, then resume the high priority task.  In this state we
		will write to the queue three times.  When the scheduler is resumed
		we expect the high priority task to service all three messages. */
		vTaskSuspendAll();
		{
			vTaskResume( xCreatedTasks[ evtLOWEST_PRIORITY_INDEX ] );
			vTaskResume( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_1 ] );

			for( xDummy = 0; xDummy < evtQUEUE_LENGTH; xDummy++ )
			{
				if( xQueueSend( xQueue, &xSendVal, evtNO_DELAY ) != pdTRUE )
				{
					xHealthStatus = pdFAIL;
					reportError();
				}
			}

			/* The queue should not have been serviced yet!.  The scheduler
			is still suspended. */
			if( memcmp(
					( void * ) xExpectedTaskCounters,
					( void * ) xTaskCounters,
					sizeof( xExpectedTaskCounters ) ) )
			{
				xHealthStatus = pdFAIL;
				reportError();
			}
		}
		xTaskResumeAll();

		/* We should have been preempted by resuming the scheduler - so by the
		time we are running again we expect the high priority task to have
		removed three items from the queue. */
		xExpectedTaskCounters[ evtHIGHEST_PRIORITY_INDEX_1 ] += evtQUEUE_LENGTH;

		if( memcmp(
				( void * ) xExpectedTaskCounters,
				( void * ) xTaskCounters,
				sizeof( xExpectedTaskCounters ) ) )
		{
			xHealthStatus = pdFAIL;
			reportError();
		}

		/* The medium priority and second high priority tasks are still
		suspended.  Make sure to resume them before starting again. */
		vTaskResume( xCreatedTasks[ evtMEDIUM_PRIORITY_INDEX ] );
		vTaskResume( xCreatedTasks[ evtHIGHEST_PRIORITY_INDEX_2 ] );

		/* Just keep incrementing to show the task is still executing. */
		xCheckVariable++;
	}
}
/*-----------------------------------------------------------*/

static void prvCheckTaskCounters(
		portBASE_TYPE xExpectedTask,
		portBASE_TYPE xIncrement )
{

	portBASE_TYPE xDummy = 0;
	static portBASE_TYPE xSendVal = MAGIC_EVENT_VALUE;

	/* Write to the queue the requested number of times.  The data written is
	not important. */
	for( xDummy = 0; xDummy < xIncrement; xDummy++ )
	{
		if( xQueueSend( xQueue, &xSendVal, evtNO_DELAY ) != pdTRUE )
		{
			/* Did not expect to ever find the queue full. */
			xHealthStatus = pdFAIL;
			reportError();
		}
	}

	/* All the tasks blocked on the queue have a priority higher than the
	controlling task.  Writing to the queue will therefore have caused this
	task to be preempted.  By the time this line executes the event task will
	have executed and incremented its counter.  Increment the expected counter
	to the same value. */
	( xExpectedTaskCounters[ xExpectedTask ] ) += xIncrement;

	/* Check the actual counts and expected counts really are the same. */
	if( memcmp(
			( void * ) xExpectedTaskCounters,
			( void * ) xTaskCounters,
			sizeof( xExpectedTaskCounters ) ) )
	{
		/* The counters were not the same.  This means a task we did not expect
		to unblock actually did unblock. */
		xHealthStatus = pdFAIL;
		reportError();
	}
}
/*-----------------------------------------------------------*/

portBASE_TYPE xAreMultiEventTasksStillRunning( void ) {

	/* Called externally to periodically check that this test is still
	operational. */

	if( xPreviousCheckVariable == xCheckVariable )
	{
		xHealthStatus = pdFAIL;
	}

	xPreviousCheckVariable = xCheckVariable;

	return xHealthStatus;
}

void vMultiEventTasksCountPrint( void ) {
	xil_printf("mEvents: %d\r\n", xCheckVariable);
	return;
}

void vMultiEventTasksCountClear( void ) {
	xCheckVariable = 0;
	xPreviousCheckVariable = 0;
}
