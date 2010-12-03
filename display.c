/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2010 Bruno Randolf (br1@einfach.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#include "display.h"
#include "main.h"


WINDOW *filter_win = NULL;
WINDOW *show_win = NULL;
static char show_win_current;
static struct timeval last_time;


/* main windows are special */
void init_display_main(void);
void update_main_win(struct packet_info *pkt, struct node_info *node);
void update_dump_win(struct packet_info* pkt);
int main_input(int c);
void print_dump_win(const char *str);

/* filter too */
void update_filter_win(void);
void filter_input(int c);

/* "standard" windows */
void update_spectrum_win(WINDOW *show_win);
void update_statistics_win(WINDOW *show_win);
void update_essid_win(WINDOW *show_win);
void update_history_win(WINDOW *show_win);
void update_help_win(WINDOW *show_win);

int spectrum_input(WINDOW *show_win, int c);


/******************* HELPERS *******************/

void
get_per_second(unsigned int bytes, unsigned int duration, int *bps, int *dps)
{
	static struct timeval last;
	static unsigned long last_bytes, last_dur;
	static int last_bps, last_dps;
	float timediff;

	/* reacalculate only every second or more */
	timediff = (the_time.tv_sec + the_time.tv_usec/1000000.0) -
		   (last.tv_sec + last.tv_usec/1000000.0);
	if (timediff >= 1.0) {
		last_dps = (1.0*(duration - last_dur)) / timediff;
		last_bps = (1.0*(bytes - last_bytes)) / timediff;
		last = the_time;
		last_dur = duration;
		last_bytes = bytes;
	}
	*bps = last_bps;
	*dps = last_dps;
}


void __attribute__ ((format (printf, 4, 5)))
print_centered(WINDOW* win, int line, int cols, const char *fmt, ...)
{
	char* buf;
	va_list ap;

	buf = malloc(cols);
	if (buf == NULL)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, cols, fmt, ap);
	va_end(ap);

	mvwprintw(win, line, cols / 2 - strlen(buf) / 2, buf);
	free(buf);
}


/******************* STATUS *******************/

static void
update_clock(time_t* sec)
{
	static char buf[9];
	strftime(buf, 9, "%H:%M:%S", localtime(sec));
	wattron(stdscr, BLACKONWHITE);
	mvwprintw(stdscr, LINES-1, COLS-9, "|%s", buf);
	wattroff(stdscr, BLACKONWHITE);
	wnoutrefresh(stdscr);
}


static void
update_mini_status(void)
{
	wattron(stdscr, BLACKONWHITE);
	mvwprintw(stdscr, LINES-1, COLS-28, conf.paused ? "|PAU" : "|   ");
	if (!conf.filter_off && (conf.do_macfilter || conf.filter_pkt != 0xffffff))
		mvwprintw(stdscr, LINES-1, COLS-24, "|FIL");
	else
		mvwprintw(stdscr, LINES-1, COLS-24, "|   ");
	mvwprintw(stdscr, LINES-1, COLS-20, "|Ch%02d",
		  channels[conf.current_channel].chan);
	wattroff(stdscr, BLACKONWHITE);
	wnoutrefresh(stdscr);
}


/******************* WINDOW MANAGEMENT / UPDATE *******************/

static void
update_show_win(void)
{
	if (show_win_current == 'e')
		update_essid_win(show_win);
	else if (show_win_current == 'h')
		update_history_win(show_win);
	else if (show_win_current == 'a')
		update_statistics_win(show_win);
	else if (show_win_current == 's')
		update_spectrum_win(show_win);
	else if (show_win_current == '?')
		update_help_win(show_win);
}


static void
show_window(char which)
{
	if (show_win != NULL && show_win_current == which) {
		delwin(show_win);
		show_win = NULL;
		show_win_current = 0;
		return;
	}
	if (show_win == NULL) {
		show_win = newwin(LINES-1, COLS, 0, 0);
		scrollok(show_win, FALSE);
	}
	show_win_current = which;

	update_show_win();
}


void
update_display(struct packet_info* pkt, struct node_info* node)
{
	/*
	 * update only in specific intervals to save CPU time
	 * if pkt is NULL we want to force an update
	 */
	if (pkt != NULL &&
	    the_time.tv_sec == last_time.tv_sec &&
	    (the_time.tv_usec - last_time.tv_usec) < conf.display_interval ) {
		/* just add the line to dump win so we don't loose it */
		update_dump_win(pkt);
		return;
	}

	update_mini_status();

	/* update clock every second */
	if (the_time.tv_sec > last_time.tv_sec)
		update_clock(&the_time.tv_sec);

	last_time = the_time;

	if (show_win != NULL)
		update_show_win();
	else
		update_main_win(pkt, node);

	if (filter_win != NULL) {
		redrawwin(filter_win);
		wnoutrefresh(filter_win);
	}

	/* only one redraw */
	doupdate();
}


/******************* INPUT *******************/

void
handle_user_input(void)
{
	int key;

	key = getch();

	if (filter_win != NULL) {
		filter_input(key);
		return;
	}

	if (show_win != NULL && show_win_current == 's') {
		if (spectrum_input(show_win, key))
			return;
	}

	if (show_win == NULL) {
		if (main_input(key))
			return;
	}

	switch(key) {
	case ' ': case 'p': case 'P':
		conf.paused = conf.paused ? 0 : 1;
		print_dump_win(conf.paused ? "\n- PAUSED -" : "\n- RESUME -");
		break;

	case 'q': case 'Q':
		finish_all(0);

	case 'r': case 'R':
		free_lists();
		essids.split_active = 0;
		essids.split_essid = NULL;
		memset(&hist, 0, sizeof(hist));
		memset(&stats, 0, sizeof(stats));
		memset(&spectrum, 0, sizeof(spectrum));
		init_channels();
		gettimeofday(&stats.stats_time, NULL);
		break;

	case '?':
	case 'e': case 'E':
	case 'h': case 'H':
	case 'a': case 'A':
	case 's': case 'S':
		show_window(tolower(key));
		break;

	case 'f': case 'F':
		if (filter_win == NULL) {
			filter_win = newwin(25, 57, LINES/2-15, COLS/2-15);
			scrollok(filter_win, FALSE);
			update_filter_win();
		}
		break;

	case KEY_RESIZE: /* xterm window resize event */
		endwin();
		init_display();
		return;
	}

	update_display(NULL, NULL);
}


/******************* INIT *******************/

void
init_display(void)
{
	initscr();
	start_color();	/* Start the color functionality */
	keypad(stdscr, TRUE);
	nonl();		/* tell curses not to do NL->CR/NL on output */
	cbreak();	/* take input chars one at a time, no wait for \n */
	curs_set(0);	/* don't show cursor */
	noecho();
	nodelay(stdscr, TRUE);

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_RED, COLOR_BLACK);
	init_pair(4, COLOR_CYAN, COLOR_BLACK);
	init_pair(5, COLOR_BLUE, COLOR_BLACK);
	init_pair(6, COLOR_BLACK, COLOR_WHITE);
	init_pair(7, COLOR_MAGENTA, COLOR_BLACK);

	init_pair(8, COLOR_GREEN, COLOR_GREEN);
	init_pair(9, COLOR_RED, COLOR_RED);
	init_pair(10, COLOR_BLUE, COLOR_BLUE);
	init_pair(11, COLOR_CYAN, COLOR_CYAN);
	init_pair(12, COLOR_YELLOW, COLOR_BLACK);
	init_pair(13, COLOR_YELLOW, COLOR_YELLOW);
	init_pair(14, COLOR_WHITE, COLOR_RED);

	/* COLOR_BLACK COLOR_RED COLOR_GREEN COLOR_YELLOW COLOR_BLUE
	COLOR_MAGENTA COLOR_CYAN COLOR_WHITE */

	erase();

	wattron(stdscr, BLACKONWHITE);
	mvwhline(stdscr, LINES-1, 0, ' ', COLS);

#define KEYMARK A_UNDERLINE
	attron(KEYMARK); printw("Q"); attroff(KEYMARK); printw("uit ");
	attron(KEYMARK); printw("P"); attroff(KEYMARK); printw("ause s");
	attron(KEYMARK); printw("O"); attroff(KEYMARK); printw("rt ");
	attron(KEYMARK); printw("F"); attroff(KEYMARK); printw("ilter ");
	attron(KEYMARK); printw("H"); attroff(KEYMARK); printw("istory ");
	attron(KEYMARK); printw("E"); attroff(KEYMARK); printw("SSIDs St");
	attron(KEYMARK); printw("a"); attroff(KEYMARK); printw("ts ");
	attron(KEYMARK); printw("R"); attroff(KEYMARK); printw("eset ");
	attron(KEYMARK); printw("S"); attroff(KEYMARK); printw("pectrum ");
	attron(KEYMARK); printw("?"); attroff(KEYMARK); printw("Help");
#undef KEYMARK
	mvwprintw(stdscr, LINES-1, COLS-15, "|%s", conf.ifname);
	wattroff(stdscr, BLACKONWHITE);

	update_mini_status();

	init_display_main();

	if (conf.do_change_channel)
		show_window('s');

	update_display(NULL, NULL);
}


void
finish_display(int sig)
{
	endwin();
}
