#include "sdp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum command_type
{
	READ_REGISTER = 0x0101,
	WRITE_REGISTER = 0x0202,
	WRITE_FILE = 0x0404,
	ERROR_STATUS = 0x0505,
	DCD_WRITE = 0x0A0A,
	JUMP_ADDRESS = 0x0B0B,
	SKIP_DCD_HEADER = 0x0C0C,
};

enum hab_status
{
	HAB_CLOSED = 0x12343412,
	HAB_OPEN = 0x56787856,
};

enum response_code
{
	WRITE_REGISTER_COMPLETE = 0x128A8A12,
	WRITE_FILE_COMPLETE = 0x88888888,
	DCD_WRITE_COMPLETE = 0x128A8A12,
	SKIP_DCD_HEADER_ACK = 0x900DD009,
};

static int write_command(hid_device *handle, enum command_type cmd, uint32_t address,
						 uint8_t format, uint32_t data_count, uint32_t data)
{
	struct
	{
		uint8_t report_id;
		uint16_t command_type;
		uint32_t address;
		uint8_t format;
		uint32_t data_count;
		uint32_t data;
		uint8_t reserved;
	} __attribute__((packed)) report1 = {
		.report_id = 1,
		.command_type = cmd,
		.address = htonl(address),
		.format = format,
		.data_count = htonl(data_count),
		.data = htonl(data),
		.reserved = 0,
	};

	int res = hid_write(handle, (const unsigned char *)&report1, sizeof(report1));
	if (res < 0)
	{
		fprintf(stderr, "ERROR: Failed to write command: %ls\n", hid_error(handle));
		return 1;
	}
	if (res != sizeof(report1))
	{
		fprintf(stderr, "ERROR: Short command write (wrote %d bytes)\n", res);
		return 1;
	}
	return 0;
}

static int read_report(hid_device *handle, uint8_t report_id, unsigned char *buf,
					   size_t length, bool optional)
{
	int res = hid_read_timeout(handle, buf, length, optional ? 500 : -1);
	if (res < 0)
	{
		if (!optional)
			fprintf(stderr, "ERROR: Failed to read report %d: %ls\n",
					report_id, hid_error(handle));
		return 1;
	}
	if ((size_t)res != length)
	{
		/* This covers the timeout case (res==0) */
		if (!optional)
			fprintf(stderr, "ERROR: Short report %d read (got=%d, wanted=%ld)\n",
					report_id, res, length);
		return 1;
	}
	if (buf[0] != report_id)
	{
		fprintf(stderr, "ERROR: Unexpected report ID (got=%d, expected=%d)\n", buf[0], report_id);
		return 1;
	}
	return 0;
}

static int read_hab_status(hid_device *handle, uint32_t *status)
{
	unsigned char buf[5];
	int res = read_report(handle, 3, buf, sizeof(buf), false);
	if (res)
		fprintf(stderr, "ERROR: Failed to read HAB status\n");
	else
	{
		uint32_t tmp = *(uint32_t *)(buf + 1);
		if (status)
			*status = tmp;
		printf("HAB: ");
		switch (tmp)
		{
		case HAB_CLOSED:
			printf("closed\n");
			break;
		case HAB_OPEN:
			printf("open\n");
			break;
		default:
			printf("unknown (0x%08x)\n", *status);
			break;
		}
	}
	return res;
}

static int read_response(hid_device *handle, uint32_t *status, bool optional)
{
	unsigned char buf[65];
	int res = read_report(handle, 4, buf, sizeof(buf), optional);
	if (res && !optional)
		fprintf(stderr, "ERROR: Failed to read response\n");
	else
	{
		uint32_t tmp = *(uint32_t *)(buf + 1);
		if (status)
			*status = tmp;
	}
	return res;
}

int sdp_write_file(hid_device *handle, const char *file_path, uint32_t address)
{
	int res;
	int fd = open(file_path, O_RDONLY);
	if (fd < 0)
	{
		fprintf(stderr, "ERROR: Failed to open file \"%s\": %s\n", file_path, strerror(errno));
		res = -1;
		goto out;
	}

	struct stat stat;
	res = fstat(fd, &stat);
	if (res)
	{
		fprintf(stderr, "ERROR: Failed to stat file \"%s\": %s\n", file_path, strerror(errno));
		goto close_fd;
	}
	printf("Writing file \"%s\" (size: %ld) to 0x%08x\n", file_path, stat.st_size, address);

	res = write_command(handle, WRITE_FILE, address, 0, stat.st_size, 0);
	if (res)
		goto close_fd;

	/*
	 * Optionally send ERROR_STATUS command here to see whether the device has
	 * rejected the address.
	 */

	/* We need one extra byte for the initial report ID */
	unsigned char buf[1025];
	buf[0] = 2;
	while (stat.st_size > 0)
	{
		ssize_t n = read(fd, buf + 1, stat.st_size > 1024 ? 1024 : stat.st_size);
		if (n <= 0)
		{
		}
		stat.st_size -= n;

		res = hid_write(handle, buf, n + 1);
		if (res < 0)
		{
			fprintf(stderr, "ERROR: Failed to write data chunk: %ls\n", hid_error(handle));
			goto close_fd;
		}
		if (res != n + 1)
		{
			fprintf(stderr, "ERROR: Short data chunk write (wrote %d bytes, wanted %ld bytes)\n", res, n);
			goto close_fd;
		}
	}

	uint32_t hab_status, status;
	res = read_hab_status(handle, &hab_status);
	if (res)
		goto close_fd;
	res = read_response(handle, &status, false);
	if (res)
		goto close_fd;
	if (status != WRITE_FILE_COMPLETE)
	{
		fprintf(stderr, "ERROR: Failed to write file: 0x%08x\n", status);
		res = 1;
	}

close_fd:
	close(fd);
out:
	return res;
}

int sdp_error_status(hid_device *handle, uint32_t *hab_status, uint32_t *status)
{
	int res = write_command(handle, ERROR_STATUS, 0x00000000, 0, 0, 0);
	if (res)
		return 1;
	res = read_hab_status(handle, hab_status);
	if (res)
		return 1;
	res = read_response(handle, status, false);
	if (res)
		return 1;
	printf("Error status: 0x%08x\n", *status);
	return 0;
}

int sdp_jump_address(hid_device *handle, uint32_t address)
{
	printf("Jumping to 0x%08x\n", address);
	int res = write_command(handle, JUMP_ADDRESS, address, 0, 0, 0);
	if (res)
		return 1;
	uint32_t hab_status, status;
	res = read_hab_status(handle, &hab_status);
	if (res)
		return 1;
	// Report 4 is only sent if the jump failed
	res = read_response(handle, &status, true);
	if (!res)
	{
		fprintf(stderr, "ERROR: Jumping to 0x%08x failed: 0x%08x\n", address, status);
		return 1;
	}
	return 0;
}
