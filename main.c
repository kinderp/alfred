#include<stdio.h>	// printf()
#include<string.h>	// strcmp()
#include<stdlib.h>	// malloc() - realpath()
#include<limits.h>	// NAME_MAX - PATH_MAX
#include<sys/inotify.h>	// inotify_init() - inotify_add_watch()
#include<unistd.h>	// read()

#include <sys/time.h>   // gettimeofday()
#include <time.h>       // localtime() - strftime()

#define BUF_LEN (10 *(sizeof(struct inotify_event) + NAME_MAX + 1))
#define FILENAME "report.txt"

int NUM_WATCHED_ITEMS;

struct watched_item_node {
	int wd;
	char abs_path[PATH_MAX];
};

struct watched_item_node * WATCHED_ITEMS;

FILE *REPORT;

char * getAbsPath(const char *relativePath, char *absolutePath) {
	return realpath(relativePath, absolutePath);
}

void getEventNameFromMask(uint32_t mask, char *dest, size_t dest_size){
    if (dest == NULL || dest_size == 0) return;
    
    dest[0] = '\0';

    if (mask & IN_CREATE) 	strncat(dest, "IN_CREATE ", dest_size - strlen(dest) - 1);
    if (mask & IN_DELETE) 	strncat(dest, "IN_DELETE ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVED_FROM) 	strncat(dest, "IN_MOVED_FROM ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVED_TO) 	strncat(dest, "IN_MOVED_TO ", dest_size - strlen(dest) - 1);
    if (mask & IN_ISDIR) 	strncat(dest, "IN_ISDIR ", dest_size - strlen(dest) - 1);
    if (mask & IN_DELETE_SELF) 	strncat(dest, "IN_DELETE_SELF ", dest_size - strlen(dest) - 1);
    if (mask & IN_IGNORED) 	strncat(dest, "IN_IGNORED ", dest_size - strlen(dest) - 1);
    
    if (dest[0] == '\0') {
        strncpy(dest, "UNRECOGNIZED", dest_size - 1);
    }
}

void processEvent(int inotifyFd, uint32_t mask, struct inotify_event *event) {
	/* 
	 * IN_ISDIR & IN_CREATE : A new dir has been created inside a watched
	 * 			  directory. We have to add a new  watch  for
	 *			  this new directory.
	 */
	if (event->mask & IN_ISDIR && event->mask & IN_CREATE){
		char abs_path[PATH_MAX];
                int n = snprintf(abs_path, PATH_MAX, "%s/%s", WATCHED_ITEMS[event->wd].abs_path, event->name);
		if (n >= sizeof(abs_path)) {
    			printf("%s is too long!\n", abs_path);
			printf("Exit...");
			exit(-1);
		}
		int wd = inotify_add_watch(inotifyFd, abs_path, mask);
		if (wd >= NUM_WATCHED_ITEMS) {
			NUM_WATCHED_ITEMS = NUM_WATCHED_ITEMS * 10;
			WATCHED_ITEMS = (struct watched_item_node *) realloc(WATCHED_ITEMS, NUM_WATCHED_ITEMS*sizeof(struct watched_item_node));
		}
                if (wd == -1){
                        printf("Error on adding watch on %s\n", abs_path);
                }else {
                        WATCHED_ITEMS[wd].wd = wd;
			snprintf(WATCHED_ITEMS[wd].abs_path, PATH_MAX, "%s", abs_path);
                        printf("Watching on %s using wd = %d\n", WATCHED_ITEMS[wd].abs_path, WATCHED_ITEMS[wd].wd);
                }
	}

	/* 
	 * IN_DELETE_SELF        : A watched dir has been deleted. We don' t  need
	 * 			   to remove any watch because kernel will do that
	 *			   for us. We just print some information about it
	 */
	if (event->mask & IN_DELETE_SELF) {
		printf("Removing watch on %s with wd = %d\n", WATCHED_ITEMS[event->wd].abs_path, WATCHED_ITEMS[event->wd].wd);
	}
	
	/* 
	 * IN_IGNORED	         : Kernel automatically removed a watch on a monitored
	 *			   directory. We have to clear our internal data struc
	 *			   tures.
	 */
	if (event->mask & IN_IGNORED) {
		printf("Removed watch on monitored %s with wd = %d because of a remove cmd\n", WATCHED_ITEMS[event->wd].abs_path, WATCHED_ITEMS[event->wd].wd);
		WATCHED_ITEMS[event->wd].abs_path[0] = '\0';	
		WATCHED_ITEMS[event->wd].wd = -1;	
		
	}
}

void displayEvent(struct inotify_event *event) {
	const int eventLen = 256;
	char eventName[eventLen];
	getEventNameFromMask(event->mask, eventName, eventLen);
	
	printf("\n");
	printf("Reading event: \n");
        printf("wd   = %d ( %s )\n", event->wd, WATCHED_ITEMS[event->wd].abs_path);
	printf("mask = %d ( %s )\n", event->mask, eventName);
        if (event->cookie) printf("cookie = %d\n", event->cookie);
	if (event->len > 0) {
		printf("name = %s\n", event->name);
	}
	printf("\n");
}

void getTime(char buffer_data[], int buf_len){
    struct timeval tv;
    struct tm *info_time;

    // Gets the current time with microsecond accuracy
    gettimeofday(&tv, NULL);
   
    // Converts seconds into a readable structure (hours, minutes, etc.) 
    info_time = localtime(&tv.tv_sec);

    // Format the date: Year-Month-Day Hours:Minutes:Seconds
    strftime(buffer_data, buf_len, "%Y-%m-%d %H:%M:%S", info_time);

    // Calculate milliseconds from microseconds (tv_usec / 1000)
    int millis = tv.tv_usec / 1000;
}

void saveEvent(struct inotify_event *event){
 	const int eventLen = 256;
        char eventName[eventLen];
        getEventNameFromMask(event->mask, eventName, eventLen);
	
	const int buf_len = 30;
	char buffer_time[buf_len];
	getTime(buffer_time, buf_len);
	
	fprintf(REPORT, "[%s] %s (%d) - %s/%s\n", buffer_time, eventName, event->mask, WATCHED_ITEMS[event->wd].abs_path, event->name);
	fflush(REPORT);
}

int main(int argc, char *argv[]){

	int inotifyFd;
	char buf[BUF_LEN];
	char *p;
	ssize_t numRead;
	struct inotify_event *event;

	uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_ISDIR | IN_IGNORED;

	NUM_WATCHED_ITEMS = argc;
	WATCHED_ITEMS = (struct watched_item_node *)malloc(NUM_WATCHED_ITEMS * sizeof(struct watched_item_node));
	if (argc < 2 || strcmp(argv[1], "--help") == 0){
		printf("%s pathname...\n", argv[0]);
		return 0;
	}

	inotifyFd = inotify_init();
	if (inotifyFd == -1){
		printf("Error on inotify initialization\n");
		printf("Exit...");
		return -1;
	}

	for (int i = 1; i < argc; i++){
		int wd = inotify_add_watch(inotifyFd, argv[i], mask);
		if (wd == -1){
			printf("Error on adding watch on %s\n", argv[i]);
		}else {
			WATCHED_ITEMS[wd].wd = wd;
			getAbsPath(argv[i], WATCHED_ITEMS[wd].abs_path);
			printf("Watching on %s using wd = %d\n", WATCHED_ITEMS[wd].abs_path, WATCHED_ITEMS[wd].wd);
		}

	}

	REPORT = fopen(FILENAME, "a");
	if (REPORT == NULL){
		printf("Error on opening %s...\n", FILENAME);
		printf("Exit...\n");
		return -1;
	}

	for(;;){
		numRead = read(inotifyFd, buf, BUF_LEN);
		if (numRead == -1){
			printf("Error on reading inotify_event\n");
			printf("Exit...");
			return -1;
		}

		if (numRead == 0){
			printf("Error on reading inotify_event 0 bytes (EOF!)\n");
			return -1;
		}

		printf("Read %ld bytes from inotify\n", numRead);	
		
		for (p = buf; p < buf + numRead; ){
			event = (struct inotify_event *) p;
			displayEvent(event);
			saveEvent(event);
			processEvent(inotifyFd, mask, event);
			p += sizeof(struct inotify_event) + event->len;	
		}
	}

}
