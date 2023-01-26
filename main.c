#include "config.h"
#include "stages.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

static void usage(const char *progname);

static const struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"wait", no_argument, NULL, 'w'},
	{"version", no_argument, NULL, 'V'},
	{0},
};

int main(int argc, char *argv[])
{
	/* Make stdout and stderr line-buffered */
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);

	int opt;
	bool initial_wait = false;

	while ((opt = getopt_long(argc, argv, "hwV", longopts, NULL)) != -1)
	{
		switch (opt)
		{
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 'w':
			initial_wait = true;
			break;
		case 'V':
			puts(VERSION);
			return EXIT_SUCCESS;
		default:
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc)
	{
		fprintf(stderr, "ERROR: Expected at least one stage\n");
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	sdp_stages *stages = sdp_parse_stages(argc - optind, argv + optind);
	if (!stages)
	{
		fprintf(stderr, "ERROR: Failed to parse stages\n");
		return EXIT_FAILURE;
	}

	int result = sdp_execute_stages(stages, initial_wait);

	sdp_free_stages(stages);

	return result;
}

static void usage(const char *progname)
{
	printf(
		"Usage: %s [OPTION]... <STAGE>...\n"
		"\n"
		"The following OPTIONs are available:\n"
		"\n"
		"  -h, --help  print this usage message\n"
		"  -V, --version  print version\n"
		"  -w, --wait  wait for the first stage\n"
		"\n"
		"The STAGEs have the following format:\n"
		"\n"
		"  <VID>:<PID>[,<STEP>...]\n"
		"    VID  USB Vendor ID as 4-digit hex number\n"
		"    PID  USB Product ID as 4-digit hex number\n"
		"\n"
		"The STEPs can be one of the following operations:\n"
		"\n"
		"  write_file:<FILE>:<ADDRESS>\n"
		"    Write the contents of FILE to ADDRESS\n"
		"  jump_address:<ADDRESS>\n"
		"    Jump to the IMX image located at ADDRESS\n",
		progname);
}
