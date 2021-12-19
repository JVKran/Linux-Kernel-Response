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

#define LED_FILE		"/dev/leds_test_module"
#define SSD_FILE		"/dev/ssd_test_module"
#define RTM_FILE		"/dev/response_module"
#define RTM_IRQ			10

#define MIN_RESP_TIME	80

static bool exit_request = false;
uint16_t highscore = UINT16_MAX;
static int fd;

enum states {IDLE, START, DELAY, COUNT, STOP};
enum states state = IDLE;
volatile int rtm_state = 0;

void exit_handler(int n, siginfo_t *info, void *unused){
	exit_request = (n == SIGINT);
	printf(" pressed so execution is interrupted!\n");
}

void response_irq(int n, siginfo_t *info, void *unused){
    if (n == 10 && info->si_int != 0) {
		rtm_state = (uint16_t)info->si_int;
        printf("Interrupt received with state %hu.\n", rtm_state);
    }
}

int control_leds(const uint32_t value){
    FILE * fp = fopen(LED_FILE, "a");
    if(fp == NULL){
        return 1;
    }

    fprintf(fp, "%i", value);
    return fclose(fp);
}

int control_ssd(const uint32_t value){
	FILE * fp = fopen(SSD_FILE, "a");
    if(fp == NULL){
        return 1;
    }

    fprintf(fp, "%i", value);
    return fclose(fp);
}

int init_rtm(int * number){
	fd = open(RTM_FILE, O_RDWR);
    if(fd < 0) {
		return fd;
    }
 
    printf("Registering application\n");
    if (ioctl(fd, _IOW('a','a',int32_t*) ,(int32_t*)number)) {
        printf("IOCTL error!\n");
        close(fd);
        exit(1);
    }
   
    printf("Waiting for interrupts.\n");
	return 0;
}

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

	// Initialize response time meter
	err = init_rtm(&number);
	if(err < 0){
		return err;
	}

	while (!exit_request){
		switch(state){
			case IDLE: {
				if(highscore != UINT16_MAX){
					control_ssd(highscore);
				} else {
					control_ssd(0);
				}
				control_leds(leds);
				if(rtm_state == 1){
					state = START;
					printf("Starting!\n");
				}
				break;
			}
			case START: {
				control_ssd(0);
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
						printf("Leds are %i!\n", leds);
						control_leds(leds);
					}
					if(rtm_state == 4){
						// TODO: read response time from register.
						response_time = time;
						if(response_time >= MIN_RESP_TIME){
							printf("Responded within %ims!\n", response_time);
							next_state = STOP;
						}
						break;
					}
					usleep(1000);
				}
				printf("Going to %i!\n", next_state);
				state = next_state;
				break;
			}
			case STOP: {
				control_ssd(response_time);
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
				control_leds(0);
				state = IDLE;
				break;
			}
		}
	}

	// Reset leds and displays if possible
	if(!err){
		err = control_leds(0);
		err |= control_ssd(0);
	}
	close(fd);
    return err;
}