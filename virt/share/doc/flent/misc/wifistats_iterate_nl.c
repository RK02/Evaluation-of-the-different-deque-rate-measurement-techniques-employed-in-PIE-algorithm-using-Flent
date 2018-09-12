/* wifistats_iterate_nl: Reliable, fast monitoring of some wifi stats using netlink
 * Author:   Dave Taht
 * Date:     7 Mar 2016
 * Copyright (C) 2018 Toke Høiland-Jørgensen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <netlink/genl/genl.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <sys/timerfd.h>

static void defaults(args *a) {
	a->filename = NULL;
	a->dev = NULL;
	a->stations = NULL;
	a->finterval=.2;
	a->count=10;
	a->interval.tv_nsec = 0;
	a->interval.tv_sec = 0;
	a->buffer = 0;
}

#define QSTRING "c:I:f:i:hb"

int process_options(int argc, char **argv, args *o)
{
	int          option_index = 0;
	int          opt = 0;
	optind       = 1;

	while(1)
	{
		opt = getopt_long(argc, argv,
				  QSTRING,
				  long_options, &option_index);
		if(opt == -1) break;

		switch (opt)
		{
		case 'c': o->count = strtoul(optarg,NULL,10);  break;
		case 'I': o->finterval = strtod(optarg,NULL); break;
		case 'f': o->filename = optarg; break;
		case 'i': o->dev = optarg; break;
		case 'b': o->buffer = 1; break;
		case '?':
		case 'h': usage(NULL); break;
		default:  usage(NULL);
		}
	}
	o->interval.tv_sec = floor(o->finterval);
	o->interval.tv_nsec = (long long) ((o->finterval - o->interval.tv_sec) * NSEC_PER_SEC);
	return 0;
}

int run(args *a)
{
	char tmpfile[] = "/tmp/wifistats_iterateXXXXXX";
	int out = a->buffer ? mkstemp(tmpfile) : STDOUT_FILENO;
	station_stats *stations = NULL;
	char *buf;
	int c;

	if(a->buffer && !out) {
		perror("Unable to create tmpfile");
		exit(-1);
	} else {
		unlink(tmpfile); // make it disappear on close
	}

	struct itimerspec new_value = {0};

	int timer = timerfd_create(CLOCK_REALTIME, 0);
	new_value.it_interval = a->interval;
	new_value.it_value = a->interval;

	char buffer[BUFFERSIZE];
	int size = 0;
	int ctr = 0;
	stations = a->stations;

	timerfd_settime(timer,0,&new_value,NULL); // relative timer

	do {
		int err;
		long long fired;
		if(read(timer,&fired,sizeof(fired))!=8) perror("reading timer");
		ctr+=fired;

		stations_reset(stations,c);
		if((size = stations_read(stations,buffer,c)) > 0) {
			err = result(out,size,BUFFERSIZE,buffer);
		} else {
			err = result(out,0,BUFFERSIZE,buffer);
			perror("reading file");
		}
		if(err<0) break;
	} while (ctr < a->count);

	if(a->buffer) {
		lseek(out, 0, SEEK_SET);
		while((size = read(out, buffer, sizeof(buffer))) > 0)
			write(STDOUT_FILENO,buffer,size);
	}

	close(out);
	close(timer);
	stations_close(stations,c);
	free(buf);
	free(stations);
	a->stations = NULL;
	return 0;
}


int main(int argc,char **argv) {
	args a;
	int status = 0;
	defaults(&a);
	process_options(argc, argv, &a);
	run(&a);
	return status;
}
