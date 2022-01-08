#include "stages.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	int status = EXIT_FAILURE;

	if (argc <= 1)
	{
		fprintf(stderr, "ERROR: Expected at least one argument\n");
		goto out;
	}
	
	sdp_stages *stages = sdp_parse_stages(argc-1, argv+1);
	if (!stages)
	{
		fprintf(stderr, "ERROR: Failed to parse stages\n");
		goto out;
	}

	int result = sdp_execute_stages(stages);
	if (result)
	{
		goto free_stages;
	}

	status = EXIT_SUCCESS;

free_stages:
	sdp_free_stages(stages);
out:
	return status;
}
