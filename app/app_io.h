#ifndef APP_IO_H
#define APP_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <curl/curl.h>

#define min(X,Y) (((X) < (Y)) ? (X) : (Y))

#define LED_FILE		"/dev/leds_module"
#define SSD_FILE		"/dev/display_module"
#define RTM_FILE		"/dev/response_module"
#define MEAS_FILE		"/dev/measurement_module"
#define SCORE_FILE      "scores.txt"
#define RTM_IRQ			10

// File descriptors
static int rtm_fd, ssd_fd, led_fd, meas_fd;

// File pointer to score file
static FILE *score_fp;

// Open all device files. Returns non-zero on error.
// Note that opened files remain opened on error.
int open_files();

// Close all files.
void close_files();

// Initialize response time meter by registering application with the
// help of Input/Output Control.
int init_rtm(int * number);

// Write passed value to leds. LED file descriptor
// must be opened. Returns non-zero value on error.
int control_leds(const uint32_t value);

// Write passed values to displays. SSD file descriptor
// must be opened. Returns non-zero value on error.
int control_ssd(const uint16_t score, const uint8_t tries);

// Write passed value to measurement pins. MEAS file descriptor
// must be opened. Returns non-zero value on error.
int control_meas(const uint16_t state);

// Append passed score of passed player to scores.txt.
int append_score(const char* player_name, const uint16_t score);

// Reads response time into passed pointer. Returns non-zero
// value on error.
int read_response_time(uint16_t* time);

// Send POST-request to thingsboard. Function has been generated
// with `--libcurl code.c` during request with curl.
int send_request(const char* server_url, const char* access_token, const char* attribute, const uint16_t value);

#endif // APP_IO_H