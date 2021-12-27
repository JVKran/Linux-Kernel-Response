#include "app_io.h"
#include <time.h>

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

    score_fp = fopen(SCORE_FILE, "a");

	return ret;
}

void close_files(){
	close(ssd_fd);
	close(led_fd);
	close(meas_fd);
	close(rtm_fd);
    fclose(score_fp);
}

int init_rtm(int * number){
    printf("Registering application\n");
    if (ioctl(rtm_fd, _IOW('a','a',int32_t*), (int32_t*)number)) {
        printf("IOCTL error!\n");
        close(rtm_fd);
        exit(1);
    }
   
    printf("Waiting for interrupts.\n");
	return 0;
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

int append_score(const uint16_t score){
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(score_fp, "Score is %i at %02d-%02d-%d %02d:%02d:%02d!\n", score, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int read_response_time(uint16_t* time){
    char buf[10];
    int read_chars = read(rtm_fd, buf, sizeof(buf));

    sscanf(buf, "%hu", time);
    return read_chars > 0 ? 0 : -1;
}

int send_request(const char* server_url, const char* access_token, const char* attribute, const uint16_t value){
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, "Content-Type:application/json");

	char payload[100], url[200];
	sprintf(payload, "{%s: %i}", attribute, value);
	sprintf(url, "%s/api/v1/%s/telemetry", server_url, access_token);

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_URL, url);
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, payload);
	curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)20);
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.78.0");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_SSH_KNOWNHOSTS, "/home/student/.ssh/known_hosts");
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(hnd, CURLOPT_FTP_SKIP_PASV_IP, 1L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;

	return (int)ret;
}
