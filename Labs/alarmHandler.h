#ifndef ALARM_HANDLER_H
#define ALARM_HANDLER_H

void setupAlarm();
void resetAlarm();
void alarmHandler(int signal);
extern int alarmEnabled;
extern int alarmCount;

#endif 
