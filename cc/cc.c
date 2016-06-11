/*
 * Copyright (c) 2008 Stefan Kempf <sisnkemp@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CMDARGS_MAX	1023
#define COMPFILES_MAX	1023
#define CPPARGS_MAX	1023
#define LDARGS_MAX	1023
#define TMPFILES_MAX	8
#define ZARGS_MAX	1023

#define PIPE	1

extern char *__progname;

static char *target = TARGET;

/* XXX: Get rid of hard-coded path. */
static char ccomp[PATH_MAX] = "/home/stefan/src/compiler/lang.c/c_";
static char ccompname[10] = "c_";

static char *cppmdopts[][9] = {
	{ "alpha", "-D__alpha", "-D__alpha_ev4__", "-D__alpha__", "-D__ELF__",
	  NULL },
	{ "amd64", "-D__x86_64", "-D__athlon", "-D__amd64__", "-D__athlon__",
	  "-D__ELF__", NULL },
	{ "arm", "-D__arm__", "-D__ELF__", NULL },
	{ "hppa", "-D__hppa", "-D__hppa__", "-D_PA_RISC_1", "-D__ELF__",
	  NULL },
	{ "i386", "-D__i386__", "-D__i386", "-D__ELF__", NULL },
	{ "m68k", "-D__m68k__", "-D__mc68000__", "-D__mc68020__", NULL },
	{ "mips64", "-D__mips64", "-D__mips64__", "-D_R4000", "-D_mips",
	  "-D_mips_r64", "-D__mips__", "-D__ELF__", NULL },
	{ "powerpc", "-D__powerpc__", "-D_ARCH_PPC", "-D__PPC", "-D__powerpc",
	  "-D__PPC__", "-D__ELF__", NULL },
	{ "sh", "-D__SH4__", "-D__sh__", NULL },
	{ "sparc", "-D__sparc__", "-D__ELF__", NULL },
	{ "sparc64", "-D__sparc", "-D__sparc__", "-D__sparc64__",
	  "-D__sparcv9__", "-D__sparc_v9__", "-D__ELF__", NULL },
	{ "vax", "-D__vax__", NULL },
	{ NULL }
};

static int cppmdidx;

struct cmd {
	char	*c_path;
	char	*c_argv[CMDARGS_MAX + 1];
	char	*c_in;
	char	*c_out;
	int	c_argc;
	int	c_firstarg;
};

struct cmdchain {
	struct	cmd *c_cmd;
	char	*c_tofree;
	int	c_infd;
	int	c_outfd;
	int	c_tmp;
	pid_t	c_pid;
};

static struct cmdchain cmds[5];
static int ncmds;

static struct cmd cppcmd = {
	"/usr/libexec/cpp",
	{ "cpp", "-D__OCC__=44", /* XXX: Kill this */ "-D__PCC=44",
	  "-D__OpenBSD__", "-D__unix__" }
};

static struct cmd ccompcmd;
static struct cmd ascmd = { "/usr/bin/as", { "as" } };
static struct cmd ldcmd = { "/usr/bin/ld", { "ld" } };

static char *compfiles[COMPFILES_MAX + 1];
static int ncompfiles;

static char *cppargs[CPPARGS_MAX + 1];
static int ncppargs;

static char *ldargs[LDARGS_MAX + 1];
static int nldargs;

static char *tmpfiles[TMPFILES_MAX];

static char *zargs[ZARGS_MAX + 1];
static int nzargs;

static char *ofile;
static int Mflag;
static int pgflag;
static int vflag;

static void cmdinit(struct cmd *);
static void cmdreset(struct cmd *);
static void cmdaddarg(struct cmd *, char *);

static void cmdchainadd(struct cmd *, char *, int);
static void cmdchainrun(void);

static void preproc(char *, char *, int, int);
static void compile(char *, char *, int, int);
static void assemble(char *, char *, int);

static int newtmp(const char *);
static void deltmp(int);
static void addarg(char **, int *, int, char *);
static void addinput(char *);
static char *newoutfile(const char *, int);
static void freeoutfile(char *);

static void cleanup(void);
static __dead void ccexit(int);
static __dead void error(int, const char *, ...);
static __dead void errorx(int, const char *, ...);

int
main(int argc, char **argv)
{
	char *base, *p;
	char *infile, *outfile;
	int i;
	int as, comp, pp;
	int asout, compout, ppout;

	int cflag = 0;
	int Eflag = 0;
	int gflag = 0;
	int staticflag = 0;
	int pthreadflag = 0;
	int Sflag = 0;

	/*
	 * TODO: Setup signal handlers, make newtmp() and deltmp() signal-safe.
	 */

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'c':
				cflag = 1;
				break;
			case 'D':
				addarg(cppargs, &ncppargs, CPPARGS_MAX,
				    argv[i]);
				break;
			case 'E':
				Eflag = 1;
				break;
			case 'f':
				break;
			case 'g':
				gflag = 1;
				break;
			case 'I':
				addarg(cppargs, &ncppargs, CPPARGS_MAX,
				    argv[i]);
				break;
			case 'M':
				Mflag = 1;
				break;
			case 'o':
				if (ofile != NULL)
					errorx(1, "outfile already specified");
				if ((ofile = argv[i + 1]) == NULL)
					warnx("no output file specified");
				else
					ofile = argv[++i];
				break;
			case 'O':
				break;
			case 'p':
				if (strcmp(&argv[i][1], "pg") == 0)
					pgflag = 1;
				else if (strcmp(&argv[i][1], "pipe") == 0)
					break;
				else
					goto bad;
				break;
			case 's':
				if (strcmp(&argv[i][1], "static") == 0)
					staticflag = 1;
				else if (strcmp(&argv[i][1], "pthread") == 0)
					pthreadflag = 1;
				else
					goto bad;
				break;
			case 'S':
				Sflag = 1;
				break;
			case 'U':
				addarg(cppargs, &ncppargs, CPPARGS_MAX,
				    argv[i]);
				break;
			case 'v':
				vflag = 1;
				break;
			case 'W':
				break;
			case 'Z':
				if (argv[i][2] == '\0')
					errorx(1, "empty -Z flag");
				argv[i][1] = '-';
				addarg(zargs, &nzargs, ZARGS_MAX, &argv[i][1]);
				break;

				/*
				 * TODO: Put them under default later, until
				 * all necessary cc options are supported.
				 */
			case 'l':
				addarg(ldargs, &nldargs, LDARGS_MAX, argv[i]);
				break;
			default:
bad:
				errorx(1, "unknown option: %s", argv[i]);
				break;
			}
			continue;
		}

		addinput(argv[i]);
	}

	for (cppmdidx = 0; cppmdopts[cppmdidx][0] != NULL; cppmdidx++) {
		if (strcmp(cppmdopts[cppmdidx][0], target) == 0)
			break;
	}
	if (cppmdopts[cppmdidx][0] == NULL)
		errorx(1, "no cpp options for %s", target);

	if (strlcat(ccomp, target, sizeof ccomp) >= sizeof ccomp)
		errorx(1, "could not create C compiler path");
	if (strlcat(ccompname, target, sizeof ccompname) >= sizeof ccompname)
		errorx(1, "could not create C compiler command name");

	if (ncompfiles == 0 && nldargs == 0)
		errorx(1, "no input files");
	if (ofile != NULL && ncompfiles > 1)
		errorx(1, "only one input file can be used with -o when "
		    "-c or -S is specified");

	ccompcmd.c_path = ccomp;
	ccompcmd.c_argv[0] = ccompname;

	cmdinit(&cppcmd);
	for (i = 1; cppmdopts[cppmdidx][i] != NULL; i++)
		cmdaddarg(&cppcmd, cppmdopts[cppmdidx][i]);
	for (i = 0; i < ncppargs; i++)
		cmdaddarg(&cppcmd, cppargs[i]);
	if (Mflag)
		cmdaddarg(&cppcmd, "-M");
	cmdinit(&cppcmd);

	cmdinit(&ccompcmd);
	if (pgflag)
		cmdaddarg(&ccompcmd, "-P");
	for (i = 0; i < nzargs; i++)
		cmdaddarg(&ccompcmd, zargs[i]);
	cmdinit(&ccompcmd);

	cmdinit(&ascmd);
	cmdinit(&ldcmd);

	for (i = 0; i < ncompfiles; i++) {
		asout = compout = ppout = -1;
		pp = comp = as = 0;
		infile = compfiles[i];
		ncmds = 0;

		p = strrchr(compfiles[i], '.') + 1;
		if ((base = strrchr(compfiles[i], '/')) == NULL)
			base = compfiles[i];
		else
			base++;
		if (*p == 'c')
			pp = comp = as = 1;
		else if (*p == 'i')
			comp = as = 1;
		else if (*p == 'S')
			pp = as = 1;
		else if (*p == 's')
			as = 1;

		if (pp && (Eflag || Mflag)) {
			preproc(compfiles[i], NULL, -1, 0);
			cmdchainrun();
			continue;
		}

		if (pp) {
			if (Sflag && !comp)
				outfile = newoutfile(base, 's');
			else {
#if !PIPE
				ppout = newtmp(compfiles[i]);
				outfile = tmpfiles[ppout];
#else
				outfile = NULL;
#endif
			}
			preproc(compfiles[i], outfile, ppout, Sflag && !comp);
			if (Sflag && !comp) {
				cmdchainrun();
				continue;
			}
			infile = outfile;
		}

		if (comp) {
			if (Sflag)
				outfile = newoutfile(base, 's');
			else {
#if !PIPE
				compout = newtmp(compfiles[i]);
				outfile = tmpfiles[compout];
#else
				outfile = NULL;
#endif
			}
			compile(infile, outfile, compout, Sflag);
			if (Sflag) {
				cmdchainrun();
				continue;
			}
			infile = outfile;
		}

		if (Sflag)
			continue;

		if (as) {
			if (cflag)
				outfile = newoutfile(base, 'o');
			else {
				asout = newtmp(compfiles[i]);
				outfile = tmpfiles[asout];
				addarg(ldargs, &nldargs, COMPFILES_MAX,
				    outfile);
			}
			assemble(infile, outfile, cflag);
			if (cflag) {
				cmdchainrun();
				continue;
			}
		}

		cmdchainrun();
	}

	if (cflag || Sflag)
		ccexit(0);

	cmdreset(&ldcmd);
	if (staticflag) 
		cmdaddarg(&ldcmd, "-static");
	else {
		cmdaddarg(&ldcmd, "--eh-frame-hdr");
		cmdaddarg(&ldcmd, "-dynamic-linker");
		cmdaddarg(&ldcmd, "/usr/libexec/ld.so");
	}

	if (pgflag)
		cmdaddarg(&ldcmd, "/usr/lib/gcrt0.o");
	else
		cmdaddarg(&ldcmd, "/usr/lib/crt0.o");

	cmdaddarg(&ldcmd, "/usr/lib/crtbegin.o");
	for (i = 0; i < nldargs; i++)
		cmdaddarg(&ldcmd, ldargs[i]);
	if (pgflag) {
		cmdaddarg(&ldcmd, "-lc_p");
		if (pthreadflag)
			cmdaddarg(&ldcmd, "-lpthread_p");
	} else {
		cmdaddarg(&ldcmd, "-lc");
		if (pthreadflag)
			cmdaddarg(&ldcmd, "-lpthread");
	}
	cmdaddarg(&ldcmd, "/usr/lib/crtend.o");
	if (ofile != NULL) {
		cmdaddarg(&ldcmd, "-o");
		cmdaddarg(&ldcmd, ofile);
	}

	ncmds = 0;
	cmdchainadd(&ldcmd, NULL, -1);
	cmdchainrun();

	ccexit(0);
}

static void
cmdinit(struct cmd *cmd)
{
	int i;

	for (i = 0; cmd->c_argv[i] != NULL; i++)
		continue;
	cmd->c_argc = cmd->c_firstarg = i;
}

static void
cmdreset(struct cmd *cmd)
{
	cmd->c_argc = cmd->c_firstarg;
	cmd->c_argv[cmd->c_firstarg] = NULL;
}

static void
cmdaddarg(struct cmd *cmd, char *arg)
{
	if (cmd->c_argc >= CMDARGS_MAX)
		errorx(1, "too many arguments for %s", cmd->c_path);
	cmd->c_argv[cmd->c_argc++] = arg;
	cmd->c_argv[cmd->c_argc] = NULL;
}

static void
cmdchainadd(struct cmd *cmd, char *tofree, int tmp)
{
	if (ncmds >= 4)
		errorx(1, "too many commands");
	cmds[ncmds].c_cmd = cmd;
	cmds[ncmds].c_tofree = tofree;
	cmds[ncmds].c_tmp = tmp;
	ncmds++;
}

static void
cmdchainrun(void)
{
#if !PIPE
	pid_t wpid;
#endif
	pid_t pid;
	int i, j, s;

#if PIPE
	int pip[2];
#endif

	struct cmd *cmd;

#if PIPE
	for (i = 0; i < ncmds - 1; i++) {
		if (pipe(pip) == -1)
			error(1, "pipe");
		cmds[i].c_outfd = pip[1];
		if (i != ncmds - 1)
			cmds[i + 1].c_infd = pip[0];
	}

	for (i = 0; i < ncmds; i++) {
		cmd = cmds[i].c_cmd;
		if (vflag) {
			for (j = 0; j < cmd->c_argc; j++) {
				fprintf(stderr, "%s", cmd->c_argv[j]);
				if (j != cmd->c_argc - 1)
					fprintf(stderr, " ");
			}
			fprintf(stderr, "\n");
		}

		/* TODO: Handle signal issues. */
		if ((pid = cmds[i].c_pid = fork()) == -1) {
			if (cmds[i].c_infd != -1)
				close(cmds[i].c_infd);
			if (cmds[i].c_outfd != -1)
				close(cmds[i].c_outfd);
			error(1, "fork");
		}
		if (pid == 0) {
			if (i != 0) {
				if (dup2(cmds[i].c_infd, STDIN_FILENO) == -1)
					_exit(3);
			}
			if (i != ncmds - 1) {
				if (dup2(cmds[i].c_outfd, STDOUT_FILENO) == -1)
					_exit(3);
			}

			for (j = 0; j < ncmds - 1; j++) {
				close(cmds[j].c_outfd);
				if (j != ncmds - 1)
					close(cmds[j + 1].c_infd);
			}
			execv(cmd->c_path, cmd->c_argv);
			_exit(2);
		}
	}

	/* Close the pipes. */
	for (i = 0; i < ncmds - 1; i++) {
		close(cmds[i].c_outfd);
		if (i != ncmds - 1)
			close(cmds[i + 1].c_infd);
	}

	i = ncmds;
	while ((pid = wait(&s)) != -1 || errno == EINTR) {
		if (pid == -1)
			continue;
		for (j = 0; j < ncmds; j++) {
			if (cmds[j].c_pid == pid) {
				cmd = cmds[j].c_cmd;
				if (!WIFEXITED(s) || WEXITSTATUS(s) != 0)
					warnx("%s terminated abnormally",
					    cmd->c_argv[0]);
				i--;
				break;
			}
		}
		if (j == ncmds)
			warnx("wait");
	}
	if (i || errno != ECHILD)
		errorx(1, "wait");

#else
	for (i = 0; i < ncmds; i++) {
		cmd = cmds[i].c_cmd;
		if (vflag) {
			for (j = 0; j < cmd->c_argc; j++) {
				fprintf(stderr, "%s", cmd->c_argv[j]);
				if (j != cmd->c_argc - 1)
					fprintf(stderr, " ");
			}
			fprintf(stderr, "\n");
		}

		/* TODO: Handle signal issues. */
		if ((pid = fork()) == -1)
			error(1, "fork");
		else if (pid == 0) {
			execv(cmd->c_path, cmd->c_argv);
			_exit(2);
		}

		/* parent */
		if ((wpid = wait(&s)) == -1)
			error(1, "wait");
		if (wpid != pid)
			errorx(1, "wait");
		if (!WIFEXITED(s) || WEXITSTATUS(s) != 0)
			errorx(1, "%s terminated abnormally", cmd->c_argv[0]);
	}
#endif

	for (i = 0; i < ncmds; i++) {
		deltmp(cmds[i].c_tmp);
		freeoutfile(cmds[i].c_tofree);
	}
}

static void
preproc(char *in, char *out, int tmp, int freeout)
{
	cmdreset(&cppcmd);
	cmdaddarg(&cppcmd, in);
	if (out != NULL)
		cmdaddarg(&cppcmd, out);
	cmdchainadd(&cppcmd, freeout ? out : NULL, tmp);
}

static void
compile(char *in, char *out, int tmp, int freeout)
{
	cmdreset(&ccompcmd);
	if (in != NULL)
		cmdaddarg(&ccompcmd, in);
	if (out != NULL)
		cmdaddarg(&ccompcmd, out);
	cmdchainadd(&ccompcmd, freeout ? out : NULL, tmp);
}

static void
assemble(char *in, char *out, int freeout)
{
	cmdreset(&ascmd);
	if (in != NULL)
		cmdaddarg(&ascmd, in);
	cmdaddarg(&ascmd, "-o");
	cmdaddarg(&ascmd, out);
	cmdchainadd(&ascmd, freeout ? out : NULL, -1);
}

static void
addarg(char **vec, int *n, int max, char *arg)
{
	if (*n >= max)
		errorx(1, "too many arguments");
	vec[(*n)++] = arg;
	vec[*n] = NULL;
}

static void
addinput(char *path)
{
	char *p;

	if ((p = strrchr(path, '.')) == NULL)
		errorx(1, "unknown input file: %s", path);
	if (p[1] == '\0' || p[2] != '\0')
		errorx(1, "unknown input file: %s", path);

	switch (p[1]) {
	case 'c':
	case 'i':
	case 's':
	case 'S':
		addarg(compfiles, &ncompfiles, COMPFILES_MAX, path);
		break;
	case 'o':
		addarg(ldargs, &nldargs, COMPFILES_MAX, path);
		break;
	default:
		errorx(1, "unknown input file: %s", path);
	}
}

static char *
newoutfile(const char *old, int suf)
{
	char *new, *p;

	if (ofile != NULL)
		return ofile;
	if ((new = strdup(old)) == NULL)
		error(1, "newoutfile: strdup");
	if ((p = strrchr(new, '.')) != NULL)
		p[1] = suf;
	return new;
}

static void
freeoutfile(char *p)
{
	if (ofile == NULL)
		free(p);
}

static int
newtmp(const char *infile)
{
	char *p, *tmpl;
	int fd, i = TMPFILES_MAX;
	size_t inlen, tmplen;

	if ((p = strrchr(infile, '/')) != NULL)
		infile = &p[1];
	inlen = strlen(infile);
	tmplen = strlen(_PATH_TMP) + 3 + 10 + inlen + 1;
	if ((tmpl = malloc(tmplen)) == NULL)
		error(1, "newtmp: xmalloc");
	*tmpl = '\0';
	strlcpy(tmpl, _PATH_TMP"cc.XXXXXXXXXX", tmplen);
	strlcat(tmpl, infile, tmplen);

	if ((fd = mkstemps(tmpl, inlen)) == -1)
		error(1, "newtmp %s", tmpl);

	/* TODO: Block signals */

	close(fd);

	for (i = 0; i < TMPFILES_MAX; i++) {
		if (tmpfiles[i] == NULL) {
			tmpfiles[i] = tmpl;
			break;
		}
	}

	/* TODO: Unblock signals */

	if (i == TMPFILES_MAX)
		errorx(1, "could not create temporary file");
	return i;
}

static void
deltmp(int i)
{
	char *p = NULL;

	if (i == -1)
		return;

	/* TODO: Block signals */

	if (tmpfiles[i] == NULL)
		goto out;
	unlink(tmpfiles[i]);
	p = tmpfiles[i];
	tmpfiles[i] = NULL;

	/* TODO: Unblock signals */

out:
	free(p);
}

static void
cleanup(void)
{
	int i;

	/* TODO: Block signals */

	for (i = 0; i < TMPFILES_MAX; i++) {
		if (tmpfiles[i] != NULL)
			unlink(tmpfiles[i]);
	}
}

static __dead void
ccexit(int eval)
{
	cleanup();
	exit(eval);
}

static __dead void
error(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	ccexit(eval);
}

static __dead void
errorx(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	ccexit(eval);
}
