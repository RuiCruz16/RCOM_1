#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "alarmHandler.h"
 
#define FALSE 0
#define TRUE 1
 
int alarmEnabled = FALSE;
int alarmCount = 0;
 
// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}
 
// Function to set up the alarm
void setupAlarm()
{
    if (alarmEnabled == FALSE)
    {
        (void)signal(SIGALRM, alarmHandler);
        alarm(3); // Set alarm to trigger in 3 seconds
        alarmEnabled = TRUE;
    }
}
 
// Function to reset the alarm
void resetAlarm()
{
    alarm(0);          // Cancel the alarm
    alarmEnabled = FALSE;
    alarmCount = 0;    // Reset the alarm count if needed
}