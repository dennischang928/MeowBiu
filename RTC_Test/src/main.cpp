#include "Arduino.h"
#include <DS3231-RTC.h>
#include <time.h>

DS3231 RTC;

void setRTCRealTime(DS3231 &RTC)
{
    int year, day, hour, month, minute, second;
    char monthStr[4];

    sscanf(__DATE__, "%3s %d %d", monthStr, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    month = (strstr(months, monthStr) - months) / 3 + 1;

    RTC.setYear(year - 2000);
    RTC.setMonth(month);
    RTC.setDate(day);
    RTC.setHour(hour);
    RTC.setMinute(minute);
    RTC.setSecond(second);

    // Zeller's congruence for day of week (1=Sunday, 7=Saturday)
    int y = year, m = month;
    if (m < 3) { m += 12; y--; }
    int dow = ((day + 2 * m + 3 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7) + 1;
    
    RTC.setDoW(dow);
}

void getRTCRealTime(DS3231 &RTC, int& year, int& month, int& day, int& dow, int& hour, int& minute, int& second, bool verbose=false)
{
    bool h12_format = false;
    bool pm_flag = false; // don't care
    bool century = false; // don't care

    year = (2000 + RTC.getYear());
    month = (RTC.getMonth(century));
    day = (RTC.getDate());
    dow = (RTC.getDoW());
    hour = (RTC.getHour(h12_format, pm_flag));
    minute = (RTC.getMinute());
    second = (RTC.getSecond());

    if(verbose==false) return;
    Serial.print(year);
    Serial.print('/');  
    Serial.print(month);
    Serial.print('/');
    Serial.print(day);
    Serial.print(' ');
    Serial.print(hour);
    Serial.print(':');
    Serial.print(minute);
    Serial.print(':');
    Serial.print(second);
    Serial.print("  DOW=");
    Serial.println(dow);
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA, SCL);
    delay(1000);
}

int year, month, day, dow, hour, minute, second;

void loop()
{
    getRTCRealTime(RTC, year, month, day, dow, hour, minute, second, true);
}
