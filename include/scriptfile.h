typedef struct {
	char *textbuf;
	unsigned int textlength;
	char *textptr;
	char *eof;
	char *filename;
	int linenum;
	long *lineoffs;
} scriptfile;

char *scriptfile_gettoken(scriptfile *sf);
int scriptfile_getnumber(scriptfile *sf, int *num);
int scriptfile_getdouble(scriptfile *sf, double *num);
int scriptfile_getstring(scriptfile *sf, char **st);
int scriptfile_getsymbol(scriptfile *sf, int *num);
int scriptfile_getlinum(scriptfile *sf, char *ptr);
int scriptfile_getbraces(scriptfile *sf, char **braceend);

scriptfile *scriptfile_fromfile(char *fn);
scriptfile *scriptfile_fromstring(char *string);
void scriptfile_close(scriptfile *sf);

int scriptfile_getsymbolvalue(char *name, int *val);
int scriptfile_addsymbolvalue(char *name, int val);
void scriptfile_clearsymbols(void);
