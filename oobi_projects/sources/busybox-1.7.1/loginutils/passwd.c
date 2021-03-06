/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <syslog.h>

#ifdef CYCLADES
static int pflg = 0;	/* -p / -P : password in the command line */
#endif

#if ENABLE_PAM
/* PAM may include <locale.h>. We may need to undefine bbox's stub define: */
#undef setlocale
#include <security/pam_appl.h>
#include <security/pam_misc.h>

static char aux_passwd[128];
static char *parm_passwd = NULL;

static struct pam_conv conv = {
	misc_conv,
	NULL
};

static int pwd_conv(int num_msg, const struct pam_message **msgm,
          struct pam_response **response, void *appdata_ptr)
{
	struct pam_response *reply;
	int count;

	/* PAM will free this later */
	reply = malloc(num_msg * sizeof(*reply));
	if (reply == NULL)
		return PAM_CONV_ERR;

	for (count = 0; count < num_msg; count++) {
		switch(msgm[count]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				free(reply);
				return PAM_CONV_ERR;
			case PAM_PROMPT_ECHO_OFF:
				if ((parm_passwd == NULL) || (*parm_passwd == 0x00)) {
					free(reply);
					return PAM_CONV_ERR;
				}
				reply[count].resp = strdup(parm_passwd);
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				reply[count].resp = strdup("");
				reply[count].resp_retcode = PAM_SUCCESS;
				break;
			default:
				free(reply);
				return PAM_CONV_ERR;
		}
	}
	*response = reply;
	return PAM_SUCCESS;
}

static void do_pam_passwd(const char *user)
{
	pam_handle_t *pamh = NULL;
	int flags = 0, ret;

#ifdef CYCLADES
	if (pflg) {
		conv.conv = pwd_conv;
		flags |= PAM_SILENT;
		if (pflg == 1) {
			if (fscanf (stdin, "%s", aux_passwd)) {
				parm_passwd = aux_passwd;
			}
		}
	}
#endif

	ret = pam_start("passwd", user, &conv, &pamh);
	if (ret != PAM_SUCCESS) {
		fprintf(stderr, "passwd: pam_start() failed, error %d\n", ret);
		exit(10);  /* XXX */
	}

	ret = pam_chauthtok(pamh, flags);
	if (ret != PAM_SUCCESS) {
		fprintf(stderr, "passwd: pam_chauthok, error %d\n",ret );
		pam_end(pamh, ret);
		exit(10);  /* XXX */
	}

	pam_end(pamh, PAM_SUCCESS);
}
#else	/* <-- ENABLE_PAM */

static void nuke_str(char *str)
{
	if (str) memset(str, 0, strlen(str));
}

static char* new_password(const struct passwd *pw, uid_t myuid, int algo)
{
	char salt[sizeof("$N$XXXXXXXX")]; /* "$N$XXXXXXXX" or "XX" */
	char *orig = (char*)"";
	char *newp = NULL;
	char *cipher = NULL;
	char *cp = NULL;
	char *ret = NULL; /* failure so far */

	if (myuid && pw->pw_passwd[0]) {
		orig = bb_askpass(0, "Old password:"); /* returns ptr to static */
		if (!orig)
			goto err_ret;
		cipher = pw_encrypt(orig, pw->pw_passwd); /* returns ptr to static */
		if (strcmp(cipher, pw->pw_passwd) != 0) {
			syslog(LOG_WARNING, "incorrect password for '%s'",
				pw->pw_name);
			bb_do_delay(FAIL_DELAY);
			puts("Incorrect password");
			goto err_ret;
		}
	}
	orig = xstrdup(orig); /* or else bb_askpass() will destroy it */
	newp = bb_askpass(0, "New password:"); /* returns ptr to static */
	if (!newp)
		goto err_ret;
	newp = xstrdup(newp); /* we are going to bb_askpass() again, so save it */
	if (ENABLE_FEATURE_PASSWD_WEAK_CHECK
	 && obscure(orig, newp, pw) && myuid)
		goto err_ret; /* non-root is not allowed to have weak passwd */

	cp = bb_askpass(0, "Retype password:");
	if (!cp)
		goto err_ret;
	if (strcmp(cp, newp)) {
		puts("Passwords don't match");
		goto err_ret;
	}

	crypt_make_salt(salt, 1, 0); /* des */
	if (algo) { /* MD5 */
		strcpy(salt, "$1$");
		crypt_make_salt(salt + 3, 4, 0);
	}
	/* pw_encrypt returns ptr to static */
	ret = xstrdup(pw_encrypt(newp, salt));
	/* whee, success! */

 err_ret:
	nuke_str(orig);
	if (ENABLE_FEATURE_CLEAN_UP) free(orig);
	nuke_str(newp);
	if (ENABLE_FEATURE_CLEAN_UP) free(newp);
	nuke_str(cipher);
	nuke_str(cp);
	return ret;
}
#endif	/* <-- !ENABLE_PAM */

int passwd_main(int argc, char **argv);
int passwd_main(int argc, char **argv)
{
	enum {
		OPT_algo = 0x1, /* -a - password algorithm */
		OPT_lock = 0x2, /* -l - lock account */
		OPT_unlock = 0x4, /* -u - unlock account */
		OPT_delete = 0x8, /* -d - delete password */
		OPT_lud = 0xe,
		STATE_ALGO_md5 = 0x10,
		//STATE_ALGO_des = 0x20, not needed yet
	};
	unsigned opt;
	int rc;
	const char *opt_a = "";
	const char *filename;
	char *myname;
	char *name;
	char *newp;
	struct passwd *pw;
	uid_t myuid;
	struct rlimit rlimit_fsize;
	char c;

#if ENABLE_FEATURE_SHADOWPASSWDS
	/* Using _r function to avoid pulling in static buffers */
	struct spwd spw;
	struct spwd *result;
	char buffer[256];
#endif

	logmode = LOGMODE_BOTH;
	openlog(applet_name, LOG_NOWAIT, LOG_AUTH);
#ifdef CYCLADES
	opt_complementary = "p--P";
	opt = getopt32(argv, "a:ludpP:", &opt_a, &parm_passwd);

	if (opt & (1<<4))
		pflg = 1;
	else if (opt & (1<<5))
		pflg = 2;
#else
	opt = getopt32(argv, "a:lud", &opt_a);
#endif

	//argc -= optind;
	argv += optind;

	if (strcasecmp(opt_a, "des") != 0) /* -a */
		opt |= STATE_ALGO_md5;
	//else
	//	opt |= STATE_ALGO_des;
	myuid = getuid();
	/* -l, -u, -d require root priv and username argument */
	if ((opt & OPT_lud) && (myuid || !argv[0]))
		bb_show_usage();

	/* Will complain and die if username not found */
	myname = xstrdup(bb_getpwuid(NULL, -1, myuid));
	name = argv[0] ? argv[0] : myname;

	pw = getpwnam(name);
	if (!pw) bb_error_msg_and_die("unknown user %s", name);
	if (myuid && pw->pw_uid != myuid) {
		/* LOGMODE_BOTH */
		bb_error_msg_and_die("%s can't change password for %s", myname, name);
	}

#if ENABLE_FEATURE_SHADOWPASSWDS
	/* getspnam_r() can lie! Even if user isn't in shadow, it can
	 * return success (pwd field was seen set to "!" in this case) */
	if (getspnam_r(pw->pw_name, &spw, buffer, sizeof(buffer), &result)
	 || LONE_CHAR(spw.sp_pwdp, '!')) {
		/* LOGMODE_BOTH */
		bb_error_msg("no record of %s in %s, using %s",
				name, bb_path_shadow_file,
				bb_path_passwd_file);
	} else {
		pw->pw_passwd = spw.sp_pwdp;
	}
#endif

	/* Decide what the new password will be */
	newp = NULL;
	c = pw->pw_passwd[0] - '!';
	if (!(opt & OPT_lud)) {
		if (myuid && !c) { /* passwd starts with '!' */
			/* LOGMODE_BOTH */
			bb_error_msg_and_die("cannot change "
					"locked password for %s", name);
		}
#ifdef ENABLE_PAM
		do_pam_passwd(name);
		return 0;
#else
		printf("Changing password for %s\n", name);
		newp = new_password(pw, myuid, opt & STATE_ALGO_md5);
		if (!newp) {
			logmode = LOGMODE_STDIO;
			bb_error_msg_and_die("password for %s is unchanged", name);
		}
#endif
	} else if (opt & OPT_lock) {
		if (!c) goto skip; /* passwd starts with '!' */
		newp = xasprintf("!%s", pw->pw_passwd);
	} else if (opt & OPT_unlock) {
		if (c) goto skip; /* not '!' */
		/* pw->pw_passwd pints to static storage,
		 * strdup'ing to avoid nasty surprizes */
		newp = xstrdup(&pw->pw_passwd[1]);
	} else if (opt & OPT_delete) {
		//newp = xstrdup("");
		newp = (char*)"";
	}

	rlimit_fsize.rlim_cur = rlimit_fsize.rlim_max = 512L * 30000;
	setrlimit(RLIMIT_FSIZE, &rlimit_fsize);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	umask(077);
	xsetuid(0);

#if ENABLE_FEATURE_SHADOWPASSWDS
	filename = bb_path_shadow_file;
	rc = update_passwd(bb_path_shadow_file, name, newp);
	if (rc == 0) /* no lines updated, no errors detected */
#endif
	{
		filename = bb_path_passwd_file;
		rc = update_passwd(bb_path_passwd_file, name, newp);
	}
	/* LOGMODE_BOTH */
	if (rc < 0)
		bb_error_msg_and_die("cannot update password file %s",
				filename);
#ifdef CYCLADES
	if (pflg != 2)
#endif
	bb_info_msg("Password for %s changed by %s", name, myname);

	//if (ENABLE_FEATURE_CLEAN_UP) free(newp);
 skip:
	if (!newp) {
		bb_error_msg_and_die("password for %s is already %slocked",
			name, (opt & OPT_unlock) ? "un" : "");
	}
	if (ENABLE_FEATURE_CLEAN_UP) free(myname);
	return 0;
}
