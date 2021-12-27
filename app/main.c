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

#include "app_io.h"

#define MIN_RESP_TIME	80
#define SERVER_URL		"https://thingsboard.jvkran.com"
#define ACCESS_TOKEN	"Response-Client"

static bool exit_request = false;
uint16_t highscore = UINT16_MAX, tries = 0;
uint32_t ssd = 0;

// Finite state machine states
enum states {IDLE, START, DELAY, COUNT, STOP};
enum states state = IDLE;
volatile int rtm_state = 0;

// Exit when CTRL+C has been pressed
void exit_handler(int n, siginfo_t *info, void *unused){
	exit_request = (n == SIGINT);
}

// Called by response time meter kernel module on state transition
// of custom platform designer component. Received state is shown
// on measurement leds.
void response_irq(int n, siginfo_t *info, void *unused){
    if (n == 10 && info->si_int != 0) {
		rtm_state = (uint16_t)info->si_int;
        printf("Interrupt received with state %hu.\n", rtm_state);
		control_meas(rtm_state);
    }
}

// Rotate (or shift) leds left.
int rotate_left(int num, int shift){
    return (num << shift) | (num >> (9 - shift));
}

int main(){
	int err = 0, number;
	struct sigaction signal;
	uint16_t response_time, leds = 0;

	// Register ctrl+c handler
	sigemptyset(&signal.sa_mask);
    signal.sa_flags = (SA_SIGINFO | SA_RESETHAND);
    signal.sa_sigaction = exit_handler;
    sigaction(SIGINT, &signal, NULL);

	// Register handler for response interrupts
	sigemptyset(&signal.sa_mask);
    signal.sa_flags = (SA_SIGINFO | SA_RESTART);
    signal.sa_sigaction = response_irq;
    sigaction(RTM_IRQ, &signal, NULL);

	// Open device files
	err = open_files();
	if(err < 0){
		printf("Opening of one or more device files failed!\n");
		close_files();
		return err;
	}

	// Initialize response time meter
	err = init_rtm(&number);
	if(err < 0){
		printf("Initializing response time meter failed!\n");
		return err;
	}

	while (!exit_request){
		switch(state){
			case IDLE: {
				if(highscore != UINT16_MAX){
					control_ssd(highscore, tries);
				} else {
					control_ssd(0, tries);
				}
				control_leds(leds);
				if(rtm_state == 1){
					state = START;
					printf("Starting!\n");
				}
				break;
			}
			case START: {
				control_ssd(0, tries);
				if(rtm_state == 3){
					state = COUNT;
					printf("Counting!\n");
				}
				break;
			}
			case COUNT: {
				enum states next_state = IDLE;
				for(uint16_t time = 0; time < 1000; time++){
					if(time % 100 == 0){
						leds <<= 1;
						leds |= 1UL;
						control_leds(leds);
					}
					if(rtm_state == 4){
						read_response_time(&response_time);
						if(response_time >= MIN_RESP_TIME){
							printf("Responded within %ims while app says %ims!\n", response_time, time);
							next_state = STOP;
						}
						break;
					}
					usleep(1000);
				}
				state = next_state;
				break;
			}
			case STOP: {
				control_ssd(response_time, tries);
				bool new_highscore = false;
				if(response_time < highscore){
					highscore = response_time;
					new_highscore = true;
				}
				while(rtm_state != 0){
					if(new_highscore){
						leds = rotate_left(leds, 1);
						control_leds(leds);
					}
					usleep(100000);			// Sleep for 100ms.
				}
				leds = 0;
				tries++;
				printf("Now %i tries!\n", tries);
				send_request(SERVER_URL, ACCESS_TOKEN, "response_time", response_time);
				append_score(response_time);
				state = IDLE;
				break;
			}
		}
	}

	// Reset leds and displays if possible
	if(!err){
		err = control_leds(0);
		err |= control_ssd(0, 0);
		err |= control_meas(8);
	}
	close_files();
    return err;
}