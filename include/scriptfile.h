typedef struct {
    char *textbuf;
    unsigned int textlength;
    char *ltextptr;     // pointer to start of the last token fetched (use this for line numbers)
    char *textptr;
    char *eof;
    char *filename;
    int linenum;
    int *lineoffs;
} scriptfile;

char *scriptfile_gettoken(scriptfile *sf);
char *scriptfile_peektoken(scriptfile *sf);
int scriptfile_getnumber(scriptfile *sf, int *num);
int scriptfile_gethex(scriptfile *sf, int *num);    // For reading specifically hex without requiring an 0x prefix
int scriptfile_getdouble(scriptfile *sf, double *num);
int scriptfile_getstring(scriptfile *sf, char **st);
int scriptfile_getsymbol(scriptfile *sf, int *num);
int scriptfile_getlinum(scriptfile *sf, char *ptr);
int scriptfile_getbraces(scriptfile *sf, char **braceend);

scriptfile *scriptfile_fromfile(const char *fn);
scriptfile *scriptfile_fromstring(const char *string);
void scriptfile_close(scriptfile *sf);
int scriptfile_eof(scriptfile *sf);

int scriptfile_getsymbolvalue(const char *name, int *val);
int scriptfile_addsymbolvalue(const char *name, int val);
void scriptfile_clearsymbols(void);
