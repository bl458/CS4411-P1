#ifndef EGOS_SIGNAL_H
#define EGOS_SIGNAL_H

typedef unsigned int sigset_t;

typedef struct {
	int		si_signo;
	int		si_errno;
	int		si_code; 
} siginfo_t;


struct sigaction {
	void		(*sa_handler)(int);
	void		(*sa_sigaction)(int, siginfo_t *, void *);
	sigset_t	sa_mask;
	int			sa_flags;
	void		(*sa_restorer)(void);
};

#endif // EGOS_SIGNAL_H
