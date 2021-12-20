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
// #include <curl/curl.h>

#define LED_FILE		"/dev/leds_test_module"
#define SSD_FILE		"/dev/ssd_test_module"
#define RTM_FILE		"/dev/response_module"
#define MEAS_FILE		"/dev/measurement_module"
#define RTM_IRQ			10

#define MIN_RESP_TIME	80
#define min(X,Y) (((X) < (Y)) ? (X) : (Y))

static bool exit_request = false;
uint16_t highscore = UINT16_MAX, tries = 0;
static int rtm_fd, ssd_fd, led_fd, meas_fd;
uint32_t ssd = 0;

enum states {IDLE, START, DELAY, COUNT, STOP};
enum states state = IDLE;
volatile int rtm_state = 0;

void exit_handler(int n, siginfo_t *info, void *unused){
	exit_request = (n == SIGINT);
}

int control_leds(const uint32_t value){
	char buf[10];
    sprintf(buf, "%i", value);

    int written_chars = write(led_fd, buf, sizeof(buf));
    return written_chars > 0 ? 0 : 1;
}

int control_ssd(const uint16_t score, const uint8_t tries){
	char buf[10];
    sprintf(buf, "%u%.4u", tries, score);

    int written_chars = write(ssd_fd, buf, sizeof(buf));
    return written_chars > 0 ? 0 : 1;
}

int control_meas(const uint16_t state){
	char buf[10];
    sprintf(buf, "%u", 1UL << state);

    int written_chars = write(meas_fd, buf, sizeof(buf));
    return written_chars > 0 ? 0 : 1;
}

int read_response_time(uint16_t* time){
    char buf[10];
    int read_chars = read(rtm_fd, buf, sizeof(buf));

    sscanf(buf, "%hu", time);
    return read_chars > 0 ? 0 : -1;
}

void response_irq(int n, siginfo_t *info, void *unused){
    if (n == 10 && info->si_int != 0) {
		rtm_state = (uint16_t)info->si_int;
        printf("Interrupt received with state %hu.\n", rtm_state);
		control_meas(rtm_state);
    }
}

int init_rtm(int * number){
    printf("Registering application\n");
    if (ioctl(rtm_fd, _IOW('a','a',int32_t*) ,(int32_t*)number)) {
        printf("IOCTL error!\n");
        close(rtm_fd);
        exit(1);
    }
   
    printf("Waiting for interrupts.\n");
	return 0;
}

int open_files(){
	int ret = 0;
	ssd_fd = open(SSD_FILE, O_WRONLY);
	ret = min(ssd_fd, ret);

	led_fd = open(LED_FILE, O_WRONLY);
	ret = min(led_fd, ret);

	meas_fd = open(MEAS_FILE, O_WRONLY);
	ret = min(meas_fd, ret);

	rtm_fd = open(RTM_FILE, O_RDWR);
	ret = min(rtm_fd, ret);

	return ret;
}

void close_files(){
	close(ssd_fd);
	close(led_fd);
	close(meas_fd);
	close(rtm_fd);
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

	// Open device files
	err = open_files();
	if(err < 0){
		printf("Opening device files failed!\n");
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