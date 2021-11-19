#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

#include "config.h"

char *mesbuf[1024];

/* Запускает процесс архивирования. */
int createArchive()
{
	static int counter = 0;
	sprintf(mesbuf, "[%u] Starting the process of creating ZIP archive", counter++);
	AddToMessageLog(mesbuf);

	char args[1024];
	sprintf(args, "WinRAR.exe a -afzip -ep1 -r -oc -o+ -u -ma5 -m5 -inul -ibck -- \"%s\\%s.zip\" @\"%s\\%s\"", config.dstPath, config.archName, confFilePath, servBackupList);

	STARTUPINFO cif;
	PROCESS_INFORMATION pi;
	ZeroMemory(&cif, sizeof(STARTUPINFO));

	// Создает новый процесс и его основной поток.
	if (CreateProcess(config.winRARexe, args, NULL, NULL, FALSE, NULL, NULL, NULL, &cif, &pi) == TRUE)
	{
		// Создание процесса  WinRAR

		// Ожидание, пока указанный объект не окажется в сигнальном состоянии или пока не истечет время ожидания.
		WaitForSingleObject(pi.hProcess, 10 * 1000);
		// Завершает указанный процесс и все его потоки.
		TerminateProcess(pi.hProcess, NO_ERROR);
		return 0;
	} else
	{
		char tmp[1024];
		sprintf(tmp, "[ERROR] Can't create WinRAR process! StartLine was: \"%s\" %s", config.winRARexe, args);
		AddToMessageLog(tmp);
		return 1;
	}
}

static int handler(void *user, const char *section, const char *name, const char *value)
{
	rawIni *pconfig = (rawIni *) user;
	
	if (MATCH("settings", "srcPath"))
	{
		pconfig->srcPath = _strdup(value);
	} else if (MATCH("settings", "dstPath"))
	{
		pconfig->dstPath = _strdup(value);
	} else if (MATCH("settings", "archName"))
	{
		pconfig->archName = _strdup(value);
	} else if (MATCH("settings", "winRARexe"))
	{
		pconfig->winRARexe = _strdup(value);
	} else if (MATCH("settings", "fileMask"))
	{
		pconfig->fileMask = _strdup(value);
	} else
	{
		return 0;  /* неизвестный раздел/название, ошибка */
	}

	return 1;
}

/* Собирает список "backup.lst" для архивации. */
int buildBackupList()
{
	const char *list_sep = " ;";

	char tmpbuf[1024];
	sprintf(tmpbuf, "%s\\%s", confFilePath, servBackupList);

	FILE *f = fopen(tmpbuf, "wb");
	if (f == NULL || _fileno(f) == -1)
	{
		sprintf(mesbuf, "[FATAL ERROR] Can't create file '%s'", tmpbuf);
		AddToMessageLog(mesbuf);
		return 1;
	}

	char *tmp = _strdup(config.fileMask);
	char *pch = strtok(tmp, list_sep);
	while (pch != NULL)
	{
		fprintf(f, "%s\\%s // mask: %s\n", config.srcPath, pch, pch);
		pch = strtok(NULL, list_sep);
	}

	if (tmp != NULL) free(tmp);
	fclose(f);

	return 0;
}

int checkConfig()
{
	if (config.srcPath == NULL || config.dstPath == NULL || config.archName == NULL || config.winRARexe == NULL || config.fileMask == NULL)
		goto ch_err;
	
	if (strcmp(config.winRARexe + strlen(config.winRARexe) - 4, ".exe") != 0)
		goto ch_err;

	char *ptr;
	ptr = config.srcPath + strlen(config.srcPath) - 1;
	if (*ptr == '\\' || *ptr == '/') *ptr = '\0';

	ptr = config.dstPath + strlen(config.dstPath) - 1;
	if (*ptr == '\\' || *ptr == '/') *ptr = '\0';

	return 0;
ch_err:
	AddToMessageLog("'config.ini' error - wrong params");
	return 1;
}

int configInit()
{
	char tmp[1024], tmp_2[1024];
	sprintf(tmp, "%s\\%s", confFilePath, servConfigIni);

	if (ini_parse(tmp, handler, &config) < 0)
	{
		sprintf(tmp_2, "[ERROR] Can't load '%s'\n", tmp);
		AddToMessageLog(tmp_2);
		return 1;
	}

	sprintf(mesbuf, "Config loaded from 'config.ini':\n\
	your file in: %s\n\
	save archive to: %s\n\
	archive name: %s\n\
	winRAR EXE location: %s\n\
	file mask: %s",
		config.srcPath, config.dstPath, config.archName, config.winRARexe, config.fileMask);
	AddToMessageLog(mesbuf);
	
	return checkConfig() || buildBackupList();
}