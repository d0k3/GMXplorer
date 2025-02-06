#ifndef NO_LUA
#include "gm9os.h"
#include "timer.h"
#include "rtc.h"
#include "ui.h"

u64 osclock;

static inline bool isLeapYear(u32 year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); 
}

size_t getWeekday(bool abbreviated, char* out, u8 weekday) {
    if (abbreviated) {
        switch(weekday) {
            case 1:
                strcat(out, "Mon");
                break;
            case 2:
                strcat(out, "Tue");
                break;
            case 3:
                strcat(out, "Wed");
                break;
            case 4:
                strcat(out, "Thu");
                break;
            case 5:
                strcpy(out, "Fri");
                break;
            case 6:
                strcat(out, "Sat");
                break;
            case 0:
            case 7:
                strcat(out, "Sun");
                break;
            default: 
                strcat(out, "");
                return 0;
        }
        return 3;
    }
    switch(weekday) {
        case 1:
            strcat(out, "Monday");
            return 6;
        case 2:
            strcat(out, "Tuesday");
            return 7;
        case 3:
            strcat(out, "Wednesday");
            return 9;
        case 4:
            strcat(out, "Thursday");
            return 8;
        case 5:
            strcpy(out, "Friday");
            return 6;
        case 6:
            strcat(out, "Saturday");
            return 8;
        case 7:
        case 0:
            strcat(out, "Sunday");
            return 6;
        default: 
            strcat(out, "");
            return 0;
    }
    return 0;
    
}

size_t getMonthName(bool abbreviated, char* out, u8 month) {
    if (abbreviated) {
        switch(month) {
            case 1:
                strcat(out, "Jan");
                break;
            case 2:
                strcat(out, "Feb");
                break;
            case 3:
                strcat(out, "Mar");
                break;
            case 4:
                strcat(out, "Apr");
                break;
            case 5:
                strcat(out, "May");
                break;
            case 6:
                strcat(out, "Jun");
                break;
            case 7:
                strcat(out, "Jul");
                break;
            case 8:
                strcat(out, "Aug");
                break;
            case 9:
                strcat(out, "Sep");
                break;
            case 10:
                strcat(out, "Oct");
                break;
            case 11:
                strcat(out, "Nov");
                break;
            case 12:
                strcat(out, "Dec");
                break; 
            default:
                strcat(out, "");
                return 0;
        }
        return 3;
    }
    switch(month) {
        case 1:
            strcat(out, "January");
            return 7;
        case 2:
            strcat(out, "February");
            return 8;
        case 3:
            strcat(out, "March");
            return 5;
        case 4:
            strcat(out, "April");
            return 5;
        case 5:
            strcat(out, "May");
            return 3;
        case 6:
            strcat(out, "Juny");
            return 4;
        case 7:
            strcat(out, "July");
            return 4;
        case 8:
            strcat(out, "August");
            return 6;
        case 9:
            strcat(out, "September");
            return 9;
        case 10:
            strcat(out, "October");
            return 7;
        case 11:
            strcat(out, "November");
            return 8;
        case 12:
            strcat(out, "December");
            return 8; 
        default:
            strcat(out, "");
            return 0;
    }
    return 0;
}

u16 getDaysMonths(u32 months, u8 years) {
    u8 daysInMonth[12] = {31, isLeapYear(2000 + years) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //is this ?: bad practice?
    u16 ret;
    for (u32 month = 0; month < months - 1; month++) {
        ret += daysInMonth[month];
    }
    return ret;
}

u64 calcUnixTime(u8 years, u8 months, u8 days, u8 hours, u8 minutes, u8 seconds) {
    u8 daysInMonth[12] = {31, isLeapYear(2000 + years) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //is this ?: bad practice?
    u32 curdays;
    u64 ret = 0;

    ret += seconds;
    ret += minutes * 60;
    ret += hours * 60 * 60;
    ret += (days - 1) * 24 * 60 * 60;

    for (u16 year = 0; year < years + 30; year++) { //+30 because unix time starting in 1970 but rtc starts in 2000
        if (isLeapYear(2000 + year)) {
            curdays = 366;
        } else {
            curdays = 365;
        }
        ret += curdays * 24 * 60 * 60;
    }

    for (u16 month = 0; month < months - 1; month++) {
        ret += daysInMonth[month] * 24 * 60 * 60;
    }

    return ret;
}

u64 getUnixTimeFromRtc(DsTime *dstime) {

    u8
    seconds = DSTIMEGET(dstime, bcd_s),
    minutes = DSTIMEGET(dstime, bcd_m),
    hours = DSTIMEGET(dstime, bcd_h),
    days = DSTIMEGET(dstime, bcd_D),
    months = DSTIMEGET(dstime, bcd_M),
    years = DSTIMEGET(dstime, bcd_Y);
    return calcUnixTime(years, months, days, hours, minutes, seconds);
    
}

u64 timer_usec( u64 start_time ) {
    return timer_ticks( start_time ) / (TICKS_PER_SEC/1000000);
}

void weekdayfix(DsTime *dstime) {
    int days = getUnixTimeFromRtc(dstime) / 86400; //days since thursday 1 1 1970
    u8 weekday = (days + 5) % 7;
    dstime->weekday = NUM2BCD(weekday);
}

void unixtodstime(u64 unixtime, DsTime *dstime) {
    u32 seconds, minutes, hours, days, year, month;
    seconds = unixtime;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;
    year = 1970;

    while(true)
    {
        bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        u16 daysInYear = leapYear ? 366 : 365;
        if(days >= daysInYear)
        {
            days -= daysInYear;
            ++year;
        }
        else
        {
            static const u8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for(month = 0; month < 12; ++month)
            {
                u8 dim = daysInMonth[month];

                if (month == 1 && leapYear)
                    ++dim;

                if (days >= dim)
                    days -= dim;
                else
                    break;
            }
            break;
        }
    }
    days++;
    month++;
    u8 bcd_year = year-2000;
    dstime->bcd_Y = NUM2BCD(bcd_year);
    dstime->bcd_M = NUM2BCD(month);
    dstime->bcd_D = NUM2BCD(days);
    dstime->bcd_h = NUM2BCD(hours);
    dstime->bcd_m = NUM2BCD(minutes);
    dstime->bcd_s = NUM2BCD(seconds);
    dstime->weekday = 0;
}

bool my_strftime(char* _out, size_t _maxsize, const char* str, DsTime *dstime) { //my refers to github.com/Gruetzig
    weekdayfix(dstime);
    size_t strl = strlen(str);
    size_t outpos = 0;
    char out[_maxsize+10];
    memset(out, 0, _maxsize+10);
    u8 minute, hour, day, month, year, weekday, second, weeknumber;
    u16 currentday, nextsunday, fyear;
    char numbuf[3], numbuf2[5], numbuf3[9], numnum1[3], numnum2[3], numnum3[3];
    if (!is_valid_dstime(dstime)) {
        return false;
    }
    for (size_t i = 0;i<strl;i++) {
        if (str[i] == '%') {
            i++;
            switch(str[i]) {
                case 'a':
                    outpos += getWeekday(true, out, DSTIMEGET(dstime, weekday));
                    break;
                case 'A':
                    outpos += getWeekday(false, out, DSTIMEGET(dstime, weekday));
                    break;
                case 'b':
                    outpos += getMonthName(true, out, DSTIMEGET(dstime, bcd_M));
                    break;
                case 'B':
                    outpos += getMonthName(false, out, DSTIMEGET(dstime, bcd_M));
                    break;
                case 'c':
                    char buf[100];
                    my_strftime(buf, _maxsize-outpos-10, "%a %b %d %X %Y", dstime);
                    strcat(out, buf);
                    outpos += strlen(buf);
                    break;
                case 'd':
                    day = DSTIMEGET(dstime, bcd_D);
                    if (day < 10) {
                        sprintf(numbuf, "0%d", day);
                    } else {
                        sprintf(numbuf, "%d", day);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'H':
                    hour = DSTIMEGET(dstime, bcd_h);
                    if (day < 10) {
                        sprintf(numbuf, "0%d", hour);
                    } else {
                        sprintf(numbuf, "%d", hour);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'I':
                    hour = DSTIMEGET(dstime, bcd_h);
                    if (hour > 12) {
                        hour = hour - 12;
                    }
                    if (!hour) {
                        hour = 12;
                    }
                    if (hour < 10) {
                        sprintf(numbuf, "0%d", hour);
                    } else {
                        sprintf(numbuf, "%d", hour);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'j':
                    currentday = getDaysMonths(DSTIMEGET(dstime, bcd_M), DSTIMEGET(dstime, bcd_Y))+DSTIMEGET(dstime, bcd_D);
                    if (currentday < 10) {
                        sprintf(numbuf, "00%d", currentday);
                    } else if (currentday < 100) {
                        sprintf(numbuf, "0%d", currentday);
                    } else {
                        sprintf(numbuf, "%d", currentday);
                    }
                    strcat(out, numbuf);
                    outpos += 3;
                    break;
                case 'm':
                    month = DSTIMEGET(dstime, bcd_M);
                    if (month < 10) {
                        sprintf(numbuf, "0%d", month);
                    } else {
                        sprintf(numbuf, "%d", month);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'M':
                    minute = DSTIMEGET(dstime, bcd_m);
                    if (minute < 10) {
                        sprintf(numbuf, "0%d", minute);
                    } else {
                        sprintf(numbuf, "%d", minute);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'p':
                    hour = DSTIMEGET(dstime, bcd_h);
                    if (hour >= 12) {
                        strcat(out, "PM");
                    } else {
                        strcat(out, "AM");
                    }
                    outpos += 2;
                    break;
                case 'S':
                    second = DSTIMEGET(dstime, bcd_m);
                    if (second < 10) {
                        sprintf(numbuf, "0%d", second);
                    } else {
                        sprintf(numbuf, "%d", second);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'U': 
                    currentday = getDaysMonths(DSTIMEGET(dstime, bcd_M), DSTIMEGET(dstime, bcd_Y))+DSTIMEGET(dstime, bcd_D);
                    weekday = DSTIMEGET(dstime, weekday);
                    nextsunday = ((7-weekday)+currentday);
                    weeknumber = (nextsunday/7)+1;
                    if (weeknumber < 10) {
                        sprintf(numbuf, "0%d", weeknumber);
                    } else {
                        sprintf(numbuf, "%d", weeknumber);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'w':
                    weekday = DSTIMEGET(dstime, weekday);
                    if (weekday == 7) {
                        weekday = 0;
                    }
                    sprintf(out, "%d", weekday);
                    strcat(out, numbuf);
                    outpos++;
                    break;
                case 'W':
                    currentday = getDaysMonths(DSTIMEGET(dstime, bcd_M), DSTIMEGET(dstime, bcd_Y))+DSTIMEGET(dstime, bcd_D);
                    weekday = DSTIMEGET(dstime, weekday);
                    nextsunday = ((8-weekday)+currentday);
                    weeknumber = (nextsunday/7);
                    if (weeknumber < 10) {
                        sprintf(numbuf, "0%d", weeknumber);
                    } else {
                        sprintf(numbuf, "%d", weeknumber);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'x': 
                    month = DSTIMEGET(dstime, bcd_M);
                    day = DSTIMEGET(dstime, bcd_D);
                    year = DSTIMEGET(dstime, bcd_Y);

                    if (month < 10) {
                        sprintf(numnum1, "0%d", month);
                    } else {
                        sprintf(numnum1, "%d", month);
                    }

                    if (day < 10) {
                        sprintf(numnum2, "0%d", day);
                    } else {
                        sprintf(numnum2, "%d", day);
                    }

                    if (year < 10) {
                        sprintf(numnum3, "0%d", year);
                    } else {
                        sprintf(numnum3, "%d", year);
                    }
                    sprintf(numbuf3, "%s/%s/%s", numnum1, numnum2, numnum3);
                    strcat(out, numbuf3);
                    outpos += 8;
                    break;
                case 'X':
                    hour = DSTIMEGET(dstime, bcd_h);
                    minute = DSTIMEGET(dstime, bcd_m);
                    second = DSTIMEGET(dstime, bcd_s);

                    if (hour < 10) {
                        sprintf(numnum1, "0%d", hour);
                    } else {
                        sprintf(numnum1, "%d", hour);
                    }

                    if (minute < 10) {
                        sprintf(numnum2, "0%d", minute);
                    } else {
                        sprintf(numnum2, "%d", minute);
                    }

                    if (second < 10) {
                        sprintf(numnum3, "0%d", second);
                    } else {
                        sprintf(numnum3, "%d", second);
                    }
                    sprintf(numbuf3, "%s:%s:%s", numnum1, numnum2, numnum3);
                    strcat(out, numbuf3);
                    outpos += 8;
                    break;
                case 'y':
                    year = DSTIMEGET(dstime, bcd_Y);
                    if (year < 10) {
                        sprintf(numbuf, "0%d", year);
                    } else {
                        sprintf(numbuf, "%d", year);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'Y':
                    fyear = DSTIMEGET(dstime, bcd_Y);
                    fyear += 2000;
                    sprintf(numbuf2, "%d", fyear);
                    strcat(out, numbuf2);
                    outpos += 4;
                    break;
                case '%':
                    strcat(out, "%");
                    outpos++;
                    break;
                default:
                    break; //not implemented


            }

        } else {
            out[outpos] = str[i];
            outpos++;
        }
        if (outpos > _maxsize) {
            break;
        }

    }
    strncpy(_out, out, _maxsize);
    return true;
}

static int os_time(lua_State *L) {
    int args = lua_gettop(L);
    u64 unixtime;
    switch(args) {
        case 0:
            DsTime dstime;
            get_dstime(&dstime);
            unixtime = getUnixTimeFromRtc(&dstime);
            lua_pushinteger(L, unixtime);
            return 1;
        case 1:
            lua_geti(L, 1, 1);
            int year = lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (year >= 2000) year -= 2000;

            lua_geti(L, 1, 2);
            int month = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_geti(L, 1, 3);
            int day = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_geti(L, 1, 4);
            int hour = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_geti(L, 1, 5);
            int minute = lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            lua_geti(L, 1, 6);
            int second = lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            lua_pushinteger(L, calcUnixTime(year, month, day, hour, minute, second));

            return 1;
        default:
            return luaL_error(L, "not a valid amount of arguments");

    }
}

static int os_date(lua_State *L) {
    DsTime dstime;
    
    get_dstime(&dstime);
    char retbuf[100];
    memset(retbuf, 0, 100);
    int args = lua_gettop(L);
    switch(args) {
        case 0:
            my_strftime(retbuf, 100, "%c", &dstime);
            lua_pushstring(L, retbuf);
            return 1;
        case 1:
            const char* str = lua_tostring(L, 1);
            if ((strcmp(str, "*t") == 0 || strcmp(str, "!*t") == 0)) {
                weekdayfix(&dstime);
                //return table with date values
                lua_newtable(L);
                //year
                lua_pushinteger(L, 2000+DSTIMEGET(&dstime, bcd_Y));
                lua_setfield(L, -2, "year");
                //month
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_M));
                lua_setfield(L, -2, "month");
                //wday (weekday)
                lua_pushinteger(L, DSTIMEGET(&dstime, weekday));
                lua_setfield(L, -2, "wday");
                //yday (yearday)
                lua_pushinteger(L, getDaysMonths(DSTIMEGET(&dstime, bcd_M), DSTIMEGET(&dstime, bcd_Y))+DSTIMEGET(&dstime, bcd_D));
                lua_setfield(L, -2, "yday");
                //day
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_D));
                lua_setfield(L, -2, "day");
                //hour
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_h));
                lua_setfield(L, -2, "hour");
                //minute
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_m));
                lua_setfield(L, -2, "min");
                //second
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_s));
                lua_setfield(L, -2, "sec");
                return 1;
            } else {
                my_strftime(retbuf, 100, lua_tostring(L, 1), &dstime);
                lua_pushstring(L, retbuf);
                return 1;
            }
        case 2:
            if (lua_tointeger(L, 2) < 946684800) { //unix timestamp is 01.01.2000 00:00:00, so everything before is previous century and not supported
                return luaL_error(L, "unix timestamp from before 2000 is not supported");
            }
            const char* str2 = lua_tostring(L, 1);
            if ((strcmp(str2, "*t") == 0 || strcmp(str2, "!*t") == 0)) {
                unixtodstime( lua_tointeger(L, 2) , &dstime);
                weekdayfix(&dstime);
                //return table with date values
                lua_newtable(L);
                //year
                lua_pushinteger(L, 2000+DSTIMEGET(&dstime, bcd_Y));
                lua_setfield(L, -2, "year");
                //month
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_M));
                lua_setfield(L, -2, "month");
                //wday (weekday)
                lua_pushinteger(L, DSTIMEGET(&dstime, weekday));
                lua_setfield(L, -2, "wday");
                //yday (yearday)
                lua_pushinteger(L, getDaysMonths(DSTIMEGET(&dstime, bcd_M), DSTIMEGET(&dstime, bcd_Y))+DSTIMEGET(&dstime, bcd_D));
                lua_setfield(L, -2, "yday");
                //day
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_D));
                lua_setfield(L, -2, "day");
                //hour
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_h));
                lua_setfield(L, -2, "hour");
                //minute
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_m));
                lua_setfield(L, -2, "min");
                //second
                lua_pushinteger(L, DSTIMEGET(&dstime, bcd_s));
                lua_setfield(L, -2, "sec");
                return 1;
            } else {
                unixtodstime( lua_tointeger(L, 2) , &dstime);
                my_strftime(retbuf, 100, str2, &dstime);
                lua_pushstring(L, retbuf);
                return 1;
            }
            
        default:
            return luaL_error(L, "not a valid amount of arguments");
    }
}

static int os_clock(lua_State *L) {
    lua_pushnumber(L, timer_usec(osclock)/10000000.0);
    return 1;
}

static int os_difftime(lua_State *L) {
    u64 t2 = lua_tointeger(L, 1);
    u64 t1 = lua_tointeger(L, 2);
    lua_pushinteger(L, t2-t1);
    return 1;
}

// os.remove and os.rename are done in data/luapackages/fs.lua
static const luaL_Reg os[] = {
    {"clock", os_clock},
    {"time", os_time},
    {"date", os_date},
    {"difftime", os_difftime},
    {NULL, NULL}
};

int gm9lua_open_os(lua_State* L) {
    luaL_newlib(L, os);
    osclock = timer_start();
    return 1;
}
#endif
