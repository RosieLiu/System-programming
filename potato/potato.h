#include <string.h>

#define BUFF_LEN   512
#define MAX_POTATO 512
#define LOC "/tmp/"

#define GO '1'
#define OVER '2'
#define BYE '3'

typedef struct potato {
	char msgType;

	int totalHops;
	int hopCount;

	unsigned long hop_trace[MAX_POTATO];
} potato_t;


void clearString( char *s ) {
	// int i;
	// for ( i=0; i < BUFF_LEN; i++ ) {
	// s[i] = '\0';
	// }
	memset((void*)s, '\0', BUFF_LEN);
}
