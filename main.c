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
#define AFILENAME "areport.txt"

int NUM_WATCHED_ITEMS;

struct watched_item_node {
	int wd;
	char abs_path[PATH_MAX];
};

#define MAX_AGGREGATED_EVENTS 2
#define MAX_PENDING_MOVES 16

struct pending_move {
    uint32_t cookie;
    int src_wd;	/* wd of the IN_MOVED_FROM ievent
		 * it will be resolved in the cor
		 * responding abs_path of the wat
		 * ched dir(inside WATCHED_ITEMS)
		 */
    char src_name[NAME_MAX]; /* name field in a IN_MOVED_FROM ievent */
};

struct pending_move moveCache[MAX_PENDING_MOVES];
int next_idx = 0;

typedef enum {
    UNDEFINED = 0,
    ADD_WATCH = 1,
    DEL_WATCH = 2,
    ADD_FILE  = 3,
    DEL_FILE  = 4,
    ADD_DIR   = 5,
    DEL_DIR   = 6,
    MOV_FILE  = 7,
    REN_FILE  = 8,
    MOV_DIR   = 9,
    REN_DIR   = 10
} event_type_t;

struct alfred_event {
	/*
	 * event_type:
	 * 1 . ADD_WATCH 
	 * 2 . DEL_WATCH
	 * 3 . ADD_FILE
	 * 4 . DEL_FILE
	 * 5 . ADD_DIR
	 * 6 . DEL_DIR
	 * 7 . MOV_FILE
	 * 8 . REN_FILE
	 * 9 . MOV_DIR
	 * 10. REN_DIR
	 */
	event_type_t type;
	char *src_path;     /* 0. abs_path IN_MOVE_FROM */
	char *src_name;     /* 1. name IN_MOVE_FROM     */
	char *dst_path;     /* 2. abs_path IN_MOVE_TO   */
	char *dst_name;     /* 3. name IN_MOVE_TO       */
};

const char* getANameFromAType(event_type_t type) {
    switch (type) {
        case ADD_WATCH: return "ADD_WATCH";
        case DEL_WATCH: return "DEL_WATCH";
        case ADD_FILE:  return "ADD_FILE";
        case DEL_FILE:  return "DEL_FILE";
        case ADD_DIR:   return "ADD_DIR";
        case DEL_DIR:   return "DEL_DIR";
        case MOV_FILE:  return "MOV_FILE";
        case REN_FILE:  return "REN_FILE";
        case MOV_DIR:   return "MOV_DIR";
        case REN_DIR:   return "REN_DIR";
        default:        return "UNDEFINED";
    }
}

void buildAEvent(struct alfred_event *aevent, event_type_t type, char *fields[]){
	aevent->type = type;
	switch (type) {
		case MOV_FILE:
		case REN_FILE:
				aevent->src_path = fields[0];
				aevent->src_name = fields[1];
				aevent->dst_path = fields[2];
				aevent->dst_name = fields[3];
				break;
		case ADD_DIR:
		case DEL_DIR:
		case ADD_FILE:
		case DEL_FILE:
				aevent->src_path = fields[0];
                                aevent->src_name = fields[1];
				aevent->dst_path = NULL;
                                aevent->dst_name = NULL;
		default:
	}	
}

struct watched_item_node * WATCHED_ITEMS;

FILE *REPORT;
FILE *AREPORT;

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

void getTimeISO8601(char *buffer, size_t buf_len) {
    struct timeval tv;
    struct tm *info_time;
    char time_string[40];

    gettimeofday(&tv, NULL);
    info_time = localtime(&tv.tv_sec);

    // Formato ISO 8601: 2023-10-27T14:30:05
    strftime(time_string, sizeof(time_string), "%Y-%m-%dT%H:%M:%S", info_time);

    // Aggiungiamo millisecondi e Offset fuso orario (es. +0200)
    // Risultato finale: 2023-10-27T14:30:05.123+0200
    snprintf(buffer, buf_len, "%s.%03d%s", 
             time_string, 
             (int)(tv.tv_usec / 1000), 
             info_time->tm_gmtoff >= 0 ? "+0100" : "+0100"); 
             // Nota: per precisione estrema sull'offset si usa %z in strftime se supportato
}

void saveAEvent(struct alfred_event *aevent) {
    char iso_time[64];
    const char *aEventName;

    getTimeISO8601(iso_time, sizeof(iso_time));
    aEventName = getANameFromAType(aevent->type);

    switch(aevent->type){
	case ADD_DIR:
                fprintf(AREPORT, "[%s] [%s] (PID:%d) new=%s/%s\n",
                        iso_time,
                        aEventName,
                        getpid(),
                        aevent->src_path,
                        aevent->src_name
                );
                fflush(AREPORT);
		break;
	case DEL_DIR:
                fprintf(AREPORT, "[%s] [%s] (PID:%d) old=%s/%s\n",
                        iso_time,
                        aEventName,
                        getpid(),
                        aevent->src_path,
                        aevent->src_name
                );
                fflush(AREPORT);
		break;

	case ADD_FILE:
		fprintf(AREPORT, "[%s] [%s] (PID:%d) new=%s/%s\n",
                        iso_time,
                        aEventName,
                        getpid(),
                        aevent->src_path,
                        aevent->src_name
                );
                fflush(AREPORT);
		break;
	case DEL_FILE:
		fprintf(AREPORT, "[%s] [%s] (PID:%d) old=%s/%s\n",
                        iso_time,
                        aEventName,
                        getpid(),
                        aevent->src_path,
                        aevent->src_name
                );
                fflush(AREPORT);
		break;
	case REN_FILE:
    		fprintf(AREPORT, "[%s] [%s] (PID:%d) from=%s/%s,to=%s/%s\n",
            		iso_time,
			aEventName,
            		getpid(),
            		aevent->src_path,
			aevent->src_name,
			aevent->dst_path,
			aevent->dst_name
		);
    		fflush(AREPORT);
		break;
	case MOV_FILE:
    		fprintf(AREPORT, "[%s] [%s] (PID:%d) from=%s/%s,to=%s/%s\n",
            		iso_time,
			aEventName,
            		getpid(),
            		aevent->src_path,
			aevent->src_name,
			aevent->dst_path,
			aevent->dst_name
		);
    		fflush(AREPORT);
		break;
	default:
		
    }
}

void saveIEvent(struct inotify_event *event) {
    char iso_time[64];
    char mask_str[256];
    
    getTimeISO8601(iso_time, sizeof(iso_time));
    getEventNameFromMask(event->mask, mask_str, sizeof(mask_str));

    // Pulizia della stringa della maschera (rimuoviamo lo spazio finale)
    size_t len = strlen(mask_str);
    if (len > 0 && mask_str[len-1] == ' ') mask_str[len-1] = '\0';

    fprintf(REPORT, "[%s] [EVENT] PID:%d WD:%d MASK:\"%s\" PATH:\"%s\" NAME:\"%s\"\n",
            iso_time,
            getpid(),
            event->wd,
            mask_str,
            WATCHED_ITEMS[event->wd].abs_path,
            (event->len > 0) ? event->name : "");
    
    fflush(REPORT);
}

void handleMoveLogic(struct inotify_event *event) {
    if (event->mask & IN_MOVED_FROM) {
        /* 
	 * Save the first piece of MOV or REN (IN_MOVED_FROM it's the first piece)
	 * We'll discriminate if it's a MOV or REN just comparing src path here in
	 * (IN_MOVED_FROM) and dst path (in IN_MOVED_TO). If they are the same it'
	 * s a REN event otherwise it's a MOV event. For better performance we' ll
	 * use wd (watch descriptor) instead of path.
	 */
        moveCache[next_idx].cookie = event->cookie;
	moveCache[next_idx].src_wd = event->wd;
        strncpy(moveCache[next_idx].src_name, event->name, NAME_MAX);
        next_idx = (next_idx + 1) % MAX_PENDING_MOVES;
    } 
    else if (event->mask & IN_MOVED_TO) {
        for (int i = 0; i < MAX_PENDING_MOVES; i++) {
            if (moveCache[i].cookie == event->cookie && event->cookie != 0) {
		/*
		 * We need to discriminate if IN_MOVED_TO is an REN or MOV (as AVENT)
		 * if abs_path of IN_MOVE_FROM and IN_MOVE_TO are the same it's a REN
		 * otherwise it's a MOV. We just check if FROM and TO wd(s) are equal
		 */
		
		// let's build aevent
		struct alfred_event aevent;
		char *fields[] = {
			WATCHED_ITEMS[moveCache[i].src_wd].abs_path,	/* 0. abs_path IN_MOVE_FROM */
			moveCache[i].src_name,				/* 1. name IN_MOVE_FROM     */
			WATCHED_ITEMS[event->wd].abs_path,		/* 2. abs_path IN_MOVE_TO   */
			event->name,	 				/* 3. name IN_MOVE_FROM     */
		};

                if (event->wd == moveCache[i].src_wd){
			buildAEvent(&aevent, REN_FILE, fields);
			saveAEvent(&aevent);
                	moveCache[i].cookie = 0; // Free space for consumed cookie
			moveCache[i].src_wd = -1;
                	return;
            	} else {
			/*
			 * if cached wd and event->wd are not equal means that
			 * IN_MOVED_FROM and IN_MOVED_TO event have been trig-
			 * gered from two different dir  so from and to paths
			 * are different (it's a MOVE) 
			 */
			buildAEvent(&aevent, MOV_FILE, fields);
			saveAEvent(&aevent);
			if (strcmp(moveCache[i].src_name, event->name) !=0) {
				/*
				 * if cached wd and event->wd are different and
				 * cached  name  and event->name are  different
				 * It's a MOVE and a RENAME at the same time
				 */
				buildAEvent(&aevent, REN_FILE, fields);
                        	saveAEvent(&aevent);	
			}
                	moveCache[i].cookie = 0; // Free space for consumed cookie
			moveCache[i].src_wd = -1;
			return;
		}
	    }
        }
        // Se arriviamo qui, il cookie non è in cache (spostato da fuori)
        printf("[EVENTO] ADD_FILE (via move-in): %s\n", event->name);
    }
}

void processEvent(int inotifyFd, uint32_t mask, struct inotify_event *event) {
	/*
	 * (cookie > 0) means we'are managing a MOVE or RENAME event:
	 * - IN_MOVE_FROM 
	 * - IN_MOVE_TO
	 * We need to aggregate above ievent correlated events in a unique one (a MOV avent). 
	 * Events aggregation will be done by handleMoveLogic()
	 */
	if (event->cookie > 0) {
        	handleMoveLogic(event);
		return;
    	}

	/* 
	 * IN_ISDIR & IN_CREATE : A new dir has been created inside a watched
	 * 			  directory. We have to add a new  watch  for
	 *			  this new directory.
	 */
	if (event->mask & IN_ISDIR && event->mask & IN_CREATE){
		char *fields[] = {
			WATCHED_ITEMS[event->wd].abs_path,	/* 0. abs_path IN_CREATE    */
			event->name,				/* 1. name IN_CREATE        */
			NULL,					/* 2. abs_path IN_MOVE_TO   */
			NULL,	 				/* 3. name IN_MOVE_FROM     */
		};
		struct alfred_event aevent;
		buildAEvent(&aevent, ADD_DIR, fields);
		saveAEvent(&aevent);
	
		char abs_path[PATH_MAX];
                int n = snprintf(abs_path, PATH_MAX, "%s/%s", WATCHED_ITEMS[event->wd].abs_path, event->name);
		if (n >= sizeof(abs_path)) {
    			printf("%s is too long!\n", abs_path);
			printf("Exit...");
			exit(-1);
		}
		int wd = inotify_add_watch(inotifyFd, abs_path, mask); /* Add watch for a new dir */
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
	} else if (event->mask & IN_CREATE) {
		char *fields[] = {
			WATCHED_ITEMS[event->wd].abs_path,	/* 0. abs_path IN_CREATE    */
			event->name,				/* 1. name IN_CREATE        */
			NULL,					/* 2. abs_path IN_MOVE_TO   */
			NULL,	 				/* 3. name IN_MOVE_FROM     */
		};
		struct alfred_event aevent;
		buildAEvent(&aevent, ADD_FILE, fields);
		saveAEvent(&aevent);
	
	}

	if (event->mask & IN_ISDIR && event->mask & IN_DELETE){
		char *fields[] = {
			WATCHED_ITEMS[event->wd].abs_path,	/* 0. abs_path IN_DELETE    */
			event->name,				/* 1. name IN_DELETE        */
			NULL,					/* 2. abs_path IN_MOVE_TO   */
			NULL,	 				/* 3. name IN_MOVE_FROM     */
		};
		struct alfred_event aevent;
		buildAEvent(&aevent, DEL_DIR, fields);
		saveAEvent(&aevent);

	} else if (event->mask & IN_DELETE){
		char *fields[] = {
			WATCHED_ITEMS[event->wd].abs_path,	/* 0. abs_path IN_DELETE    */
			event->name,				/* 1. name IN_CREATE        */
			NULL,					/* 2. abs_path IN_MOVE_TO   */
			NULL,	 				/* 3. name IN_MOVE_FROM     */
		};
		struct alfred_event aevent;
		buildAEvent(&aevent, DEL_FILE, fields);
		saveAEvent(&aevent);
	}
	/* 
	 * IN_DELETE_SELF        : A watched dir has been deleted. We don' t  need
	 * 			   to remove any watch because kernel will do that
	 *			   for us. We just print some information about it
	 *			   I Know we need to remove watch infos in WATCHED
	 *			   _ITEMS but we'll do it on IN_INGNED event below
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

	// TODO: make a functoin to open any report
	REPORT = fopen(FILENAME, "a");
	if (REPORT == NULL){
		printf("Error on opening %s...\n", FILENAME);
		printf("Exit...\n");
		return -1;
	}
	AREPORT = fopen(AFILENAME, "a");
	if (AREPORT == NULL){
		printf("Error on opening %s...\n", AFILENAME);
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
			printf("Exit...");
			return -1;
		}

		printf("Read %ld bytes from inotify\n", numRead);	
		
		for (p = buf; p < buf + numRead; ){
			event = (struct inotify_event *) p;
			displayEvent(event);
			processEvent(inotifyFd, mask, event);
			saveIEvent(event);
			p += sizeof(struct inotify_event) + event->len;	
		}
	}
}
