#include<stdio.h>	// printf() - scanf()
#include<string.h>	// strcmp()
#include<sys/inotify.h>	// inotify_init() -
#include<limits.h>	// NAME_MAX
#include<unistd.h>	// read()
#define BUF_LEN (10*(sizeof(struct inotify_event) + NAME_MAX + 1))

int main(int argc, char *argv[]){

	int inotifyFd;
        char *p;
        ssize_t numRead;
        struct inotify_event *event;
	char buf[BUF_LEN];
	
	if(argc < 2 || strcmp(argv[1], "--help")==0){
		printf("%s pathname...\n", argv[0]);
		return 0;	
	}

	inotifyFd= inotify_init();
	if(inotifyFd == -1){
                printf("Error on inotify initialization\n");
                printf("Exit...");
                return -1;
        }

	for(int i=1; i<argc; i++){
		uint32_t mask = IN_CREATE | IN_DELETE;
		int wd = inotify_add_watch(inotifyFd, argv[i], mask);

               	if(wd == -1){
                        printf("Error on adding watch on %s\n", argv[i]);
                }else {
                        printf("Watching on %s using wd = %d\n", argv[i], wd);
                }

	}

	for(; ;){
		numRead = read(inotifyFd, buf, BUF_LEN);

		if(numRead == -1){
                        printf("Error on reading inotify_event\n");
                        printf("Exit...");
                        return -1;
                }
 		
		printf("Read %ld bytes from inotify\n", numRead);
	
                for(p = buf; p < buf + numRead; ){
                        event = (struct inotify_event *) p;
                        printf("Reading event: \n");
                        printf("wd   = %d\n", event->wd);
                        if (event->mask & IN_CREATE) printf("mask = %d ( IN_CREATE )\n", event->mask);
                        if (event->mask & IN_DELETE) printf("mask = %d\n ( IN_DELETE )\n", event->mask);
                        printf("name = %s\n", event->name);

                        p += sizeof(struct inotify_event) + event->len;
                }

	}

}
