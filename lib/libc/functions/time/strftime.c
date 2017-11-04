#include <time.h>
#include <stdio.h>

// TODO localize me
static const char* wday_short[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* wday_full[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char* mon_short[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char* mon_full[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
static const char* am_pm[] = { "am", "pm" };

struct STATE {
	char* ptr;
	size_t left;
	size_t written;
};

static void append1(struct STATE* state, char ch)
{
	state->written++;
	if (state->left <= 0)
		return;
	*state->ptr++ = ch;
	state->left--;
}

static void append(struct STATE* state, const char* s)
{
	while(*s != '\0') {
		append1(state, *s);
		s++;
	}
}

size_t strftime(char* s, size_t maxsize, const char *format, const struct tm* tm)
{
	struct STATE st;
	st.ptr = s;
	st.left = maxsize;
	st.written = 0;

	char tmp[64];
	tmp[sizeof(tmp) - 1] = '\0';
	for(/* nothing */; *format != '\0'; format++) {
		/* non-% need to be copied */
		if (*format != '%') {
			append1(&st, *format);
			continue;
		}

		format++;
		if (*format == '0' || *format == 'E')
			format++; /* don't support alternative conversions */
		switch(*format) {
			case 'a':
				append(&st, (tm->tm_wday >= 0 && tm->tm_wday <= 6) ? wday_short[tm->tm_wday] : "???");
				break;
			case 'A':
				append(&st, (tm->tm_wday >= 0 && tm->tm_wday <= 6) ? wday_full[tm->tm_wday] : "???");
				break;
			case 'b':
			case 'h':
				append(&st, (tm->tm_mon >= 0 && tm->tm_wday <= 11) ? mon_short[tm->tm_mon] : "???");
				break;
			case 'B':
				append(&st, (tm->tm_mon >= 0 && tm->tm_wday <= 11) ? mon_full[tm->tm_mon] : "???");
				break;
			case 'c': /* Replaced by the locale's appropriate date and time representation. (See the Base Definitions volume of IEEE Std 1003.1-2001, <time.h>.) */
				append(&st, "[todo %c]");
				break;
			case 'C':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_year % 100);
				append(&st, tmp);
				break;
			case 'd':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_mday);
				append(&st, tmp);
				break;
			case 'D':
				snprintf(tmp, sizeof(tmp) - 1, "%02d/%02d/%02d", tm->tm_year, tm->tm_mday, tm->tm_mon);
				append(&st, tmp);
				break;
			case 'e':
				snprintf(tmp, sizeof(tmp) - 1, "%2d", tm->tm_mday);
				append(&st, tmp);
				break;
			case 'F':
			case 'x': /* locale's appropriate date representation */
				snprintf(tmp, sizeof(tmp) - 1, "%d-%02d-%02d", tm->tm_year, tm->tm_mon, tm->tm_mday);
				append(&st, tmp);
				break;
			case 'g': /* Replaced by the last 2 digits of the week-based year (see below) as a decimal number [00,99]. [ tm_year, tm_wday, tm_yday] */
				append(&st, "[todo %g]");
				break;
			case 'G': /* Replaced by the week-based year (see below) as a decimal number (for example, 1977). [ tm_year, tm_wday, tm_yday] */
				append(&st, "[todo %G]");
				break;
			case 'H':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_hour);
				append(&st, tmp);
				break;
			case 'I': {
				int h = tm->tm_hour % 12;
				if (h == 0)
					h = 12;
				snprintf(tmp, sizeof(tmp) - 1, "%02d", h);
				append(&st, tmp);
				break;
			}
			case 'j':
				snprintf(tmp, sizeof(tmp) - 1, "%03d", tm->tm_yday);
				append(&st, tmp);
				break;
			case 'm':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_mon);
				append(&st, tmp);
				break;
			case 'M':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_min);
				append(&st, tmp);
				break;
			case 'n':
				append1(&st, '\n');
				break;
			case 'p':
				append(&st, am_pm[tm->tm_hour >= 12]);
				break;
			case 'r': {
				int h = tm->tm_hour % 12;
				if (h == 0)
					h = 12;
				snprintf(tmp, sizeof(tmp) - 1, "%02d:%02d:%02d %s", h, tm->tm_min, tm->tm_sec, am_pm[tm->tm_hour >= 12]);
				append(&st, tmp);
				break;
			}
			case 'R':
				snprintf(tmp, sizeof(tmp) - 1, "%02d:%02d", tm->tm_hour, tm->tm_min);
				append(&st, tmp);
				break;
			case 'S':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_sec);
				append(&st, tmp);
				break;
			case 't':
				append1(&st, '\t');
				break;
			case 'T':
			case 'X': /* locale's appropriate time representation */
				snprintf(tmp, sizeof(tmp) - 1, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
				append(&st, tmp);
				break;
			case 'u': {
				int wday = tm->tm_wday;
				if (wday == 0)
					wday = 7;
				snprintf(tmp, sizeof(tmp) - 1, "%d", wday);
				append(&st, tmp);
				break;
			}
			case 'U': /* Replaced by the week number of the year as a decimal number [00,53]. The first Sunday of January is the first day of week 1; days in the new year before this are in week 0. [ tm_year, tm_wday, tm_yday] */
				append(&st, "[todo %U]");
				break;
			case 'V': /* Replaced by the week number of the year (Monday as the first day of the week) as a decimal number [01,53]. If the week containing 1 January has four or more days in the new year, then it is considered week 1. Otherwise, it is the last week of the previous year, and the next week is week 1. Both January 4th and the first Thursday of January are always in week 1. [ tm_year, tm_wday, tm_yday] */
				append(&st, "[todo %U]");
				break;
			case 'w':
				snprintf(tmp, sizeof(tmp) - 1, "%d", tm->tm_wday);
				append(&st, tmp);
				break;
			case 'W': /* Replaced by the week number of the year as a decimal number [00,53]. The first Monday of January is the first day of week 1; days in the new year before this are in week 0. [ tm_year, tm_wday, tm_yday] */
				break;
			case 'y':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_year % 100);
				append(&st, tmp);
				break;
			case 'Y':
				snprintf(tmp, sizeof(tmp) - 1, "%02d", tm->tm_year);
				append(&st, tmp);
				break;
			case 'z': /* TODO needs time zone support */
			case 'Z': /* TODO needs time zone support */
				break;
			default:
			case '%':
				append1(&st, *format);
				break;
		}
	}

	return st.written;
}
