#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "libs/inih-r42/ini.h"
#include "service.h"

/* Конфиг- и лог- файлы и их расположение. */
static const char *confFilePath = "C:\\Users\\Arina\\Desktop\\Mine&Study\\5 semester\\2019\\Безопасность современных информационных технологий 5 сем\\Отчеты\\Лабораторная 3\\Lab_3\\Lab_3";
static const char *servConfigIni = "config.ini";
static const char *servBackupList = "backup.lst";
static const char *servLog = "service.log";

typedef struct
{
	char *srcPath;
	char *dstPath;
	char *archName;
	char *winRARexe;
	char *fileMask;
} rawIni;

rawIni config;

int configInit();
int createArchive();

#endif