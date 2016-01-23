#ifndef _Logging_h
#define _Logging_h

#ifndef _Time_h
#include <TimeLib.h>
#endif

/*
 * Logging
 */
int LOG_LEVEL = 0;
const int LOG_DEBUG = 0;
const int LOG_INFO = 1;
const int LOG_WARN = 2;
const int LOG_ERROR = 3;
const char* states[] = {"DEBUG"," INFO"," WARN","ERROR"};

unsigned long milliOffset = 0;
unsigned long getMillisSec()
{
  //millisec since start of program
  unsigned long m = millis() - milliOffset;
  unsigned long n = m % 1000;
  return n;
}

String getDateTime(const unsigned long t, const unsigned long millisec) {
  char logBuffer[256];
  sprintf(logBuffer, "%04d-%02d-%02d %02d:%02d:%02d.%03d", year(t), month(t), day(t), hour(t), minute(t), second(t), millisec);  
  return String(logBuffer);
}
String getDateTime(const unsigned long t) {  return getDateTime(t, 0);}
String getDateTime() { return getDateTime(now(), getMillisSec());}

void logTime()
{
  if (timeStatus() != timeNotSet) {
    Serial.print("["+getDateTime()+"] ");  
  }
  else {
    Serial.print("[-- no time available --] ");
  }
}
void log(const int state, const char* text)
{
  if (state < LOG_LEVEL) return;
  logTime();
  Serial.print(states[state]);
  Serial.print(": ");
  Serial.println(text);
}
void log(const int state, const String& text)
{
  if (state < LOG_LEVEL) return;
  int l = text.length()+1;
  char b[l];
  text.toCharArray(b,l);
  log(state,b);
}
void logDebug(const char* text){log(LOG_DEBUG, text);}
void logInfo(const char* text){log(LOG_INFO, text);}
void logWarning(const char* text){log(LOG_WARN, text);}
void logError(const char* text){log(LOG_ERROR, text);}
void logDebug(const String text){log(LOG_DEBUG, text);}
void logInfo(const String text){log(LOG_INFO, text);}
void logWarning(const String text){log(LOG_WARN, text);}
void logError(const String text){log(LOG_ERROR, text);}
//Logging ends

#endif
