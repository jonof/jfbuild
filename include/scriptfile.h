typedef struct {
	char *textbuf;
	unsigned int textlength;
	char *textptr;
	char *filename;
	int linenum;
} scriptfile;

char *scriptfile_gettoken(scriptfile *sf);
int scriptfile_getnumber(scriptfile *sf, int *num);
int scriptfile_getdouble(scriptfile *sf, double *num);
char *scriptfile_getstring(scriptfile *sf);
int scriptfile_getsymbol(scriptfile *sf, int *num);

scriptfile *scriptfile_fromfile(char *fn);
scriptfile *scriptfile_fromstring(char *string);
void scriptfile_close(scriptfile *sf);

int scriptfile_getsymbolvalue(char *name, int *val);
int scriptfile_addsymbolvalue(char *name, int val);
void scriptfile_clearsymbols(void);
