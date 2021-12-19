#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define LED_FILE	"/dev/leds_test_module"
#define SSD_FILE	"/dev/ssd_test_module"
#define RTM_FILE	"/dev/response_module"
#define RTM_IRQ		10

static bool exit_request = false;
static int fd;

void exit_handler(int n, siginfo_t *info, void *unused){
	exit_request = (n == SIGINT);
	printf(" pressed so execution is interrupted!\n");
}

void response_irq(int n, siginfo_t *info, void *unused){
    if (n == 10 && info->si_int != 0) {
        printf("Interrupt received with state %i.\n", info->si_int);
    }
}

int control_leds(const int value){
    FILE * fp = fopen(LED_FILE, "a");
    if(fp == NULL){
        return 1;
    }

    fprintf(fp, "%i", value);
    return fclose(fp);
}

int control_ssd(const int value){
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
		printf("Device bestand niet kunnen openen..\n");
		return fd;
    }
 
    printf("Applicatie registreren\n");
    if (ioctl(fd, _IOW('a','a',int32_t*) ,(int32_t*)number)) {
        printf("IOCTL magie fout\n");
        close(fd);
        exit(1);
    }
   
    printf("Wacht op signaal...\n");
	return 0;
}

int main(){
	int err = 0, number;
	struct sigaction signal;

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

	init_rtm(&number);

	// Count up to 16 or until exit request
    for(int counter = 0; counter < 100 && !exit_request; counter++){
		printf("Showing %i on leds and display.\n", counter);
        err = control_leds(counter);
		err |= control_ssd(counter);
        if(err != 0){
            break;
        }
        usleep(100000);
    }

	// Reset leds and displays if possible
	if(!err){
		err = control_leds(0);
		err |= control_ssd(0);
	}
	close(fd);
    return err;
}