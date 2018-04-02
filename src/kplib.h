	//High-level (easy) picture loading function:
extern void kpzload (const char *, intptr_t *, int *, int *, int *);
	//Low-level PNG/JPG functions:
extern void kpgetdim (void *, int, int *, int *);
extern int kprender (void *, int, void *, int, int, int, int, int);

	//ZIP functions:
extern int kzaddstack (const char *);
extern void kzuninit (void);
extern int kzopen (const char *);
extern int kzread (void *, int);
extern int kzfilelength (void);
extern int kzseek (int, int);
extern int kztell (void);
extern int kzgetc (void);
extern int kzeof (void);
extern void kzclose (void);

extern void kzfindfilestart (const char *); //pass wildcard string
extern int kzfindfile (char *); //you alloc buf, returns 1:found,0:~found

