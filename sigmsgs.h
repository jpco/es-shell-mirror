/* sigmsgs.h -- interface to signal name and message date (generated by mksignal) */

typedef struct {
	int sig;
	const char *name, *msg;
} Sigmsgs;
extern const Sigmsgs signals[];

extern const int nsignals;
