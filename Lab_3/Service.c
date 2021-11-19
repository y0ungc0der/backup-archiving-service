#define _CRT_SECURE_NO_WARNINGS
#include "service.h"

SERVICE_STATUS          g_ServiceStatus = { 0 };  // Cтруктура, которая будет использоваться для сообщения о состоянии службы диспетчеру управления службами Windows (SCM).
SERVICE_STATUS_HANDLE   g_StatusHandle = NULL;    // Используется для ссылки на наш экземпляр службы после его регистрации в SCM
DWORD                   dwErr = 0;
BOOL                    bDebug = FALSE;
TCHAR                   sError[256];

// Служба должна завершиться.
HANDLE hServiceStopEvent = NULL; // Дескриптор объекта события.

/* Главная точка входа. */
int _tmain(int argc, char **argv)
{
	setlocale(LC_ALL, "rus");

	// Определяет функцию ServiceMain для службы. Используется функцией StartServiceCtrlDispatcher.
	// Члены последней записи в таблице должны иметь значения NULL для обозначения конца таблицы.
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{TEXT(SZSERVICENAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL} // больше сервисов нет
	};

	if ((argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')))
	{
		if (_stricmp("install", argv[1] + 1) == 0)
		{
			InstallService();
		}
		else if (_stricmp("remove", argv[1] + 1) == 0)
		{
			RemoveService();
		}
		else if (_stricmp("debug", argv[1] + 1) == 0)
		{
			bDebug = TRUE;
			DebugService(argc, argv);
		}
		else if (_stricmp("run", argv[1] + 1) == 0)
		{
			RunService();
		}
		else if (_stricmp("stop", argv[1] + 1) == 0)
		{
			StopService();
		}
		else
		{
			goto command;
		}
		exit(0);
	}

command:
	printf("%s -install          to install the service\n", SZAPPNAME);
	printf("%s -remove           to remove the service\n", SZAPPNAME);
	printf("%s -run              to run service\n", SZAPPNAME);
	printf("%s -stop             to stop service\n", SZAPPNAME);
	printf("%s -debug <params>   to run as a console app for debugging\n", SZAPPNAME);
	printf("\nStartServiceCtrlDispatcher being called.\n");
	printf("This may take several seconds. Please wait.\n");

	// Соединяет основной поток процесса службы с менеджером управления службами.
	if (!StartServiceCtrlDispatcher(ServiceTable))
		AddToMessageLog(TEXT("StartServiceCtrlDispatcher failed."));
	else printf("StartServiceCtrlDispatcher returned!\n");

	return 0;
}

/* Точка входа в сервис. */
void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// Битовая маска. Содержит информацию о состоянии сервиса.
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	// тип сервиса 
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // сервис является самостоятельным процессом
	// коды управляющих команд, которые могут быть переданы обработчику этих команд
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN; // можно остановить сервис, сервис информируется о выключении системы
	// текущее состояние сервиса
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING; // сервис стартует
	// Если в этом поле установлено значение ERROR_SERVICE_SPECIFIC_ERROR, то код самой ошибки находится в следующем поле dwServiceSpecificExitCode
	g_ServiceStatus.dwWin32ExitCode = 0;
	// код возврата из сервиса, если в dwWin32ExitCode установлено значение ERROR_SERVICE_SPECIFIC_ERROR
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	// это значение не используется 
	g_ServiceStatus.dwCheckPoint = 0;
	g_ServiceStatus.dwWaitHint = 0;

	// Регистрирут обработчик управления службами в SCM.
	g_StatusHandle = RegisterServiceCtrlHandler(TEXT(SZSERVICENAME), ServiceCtrlHandler);

	if (!g_StatusHandle)
		goto EXIT;

	// Сообщает SCM текущий статус сервиса.
	if (!ReportStatusToSCMgr(
		SERVICE_START_PENDING, // состояние сервиса
		NO_ERROR,              // код выхода
		3000))                 // ожидание
		goto EXIT;

	ServiceStart(dwArgc, lpszArgv);

EXIT:
	// Попытка сообщить о состоянии остановки SCM.
	if (g_StatusHandle)
		(VOID)ReportStatusToSCMgr(
			SERVICE_STOPPED,
			dwErr,
			0);

	return;
}

/* Обработчик управления службами, вызывается каждый раз, как SCM шлет запросы на изменение состояния сервиса.*/
VOID WINAPI ServiceCtrlHandler(DWORD dwCtrlCode)
{

	switch (dwCtrlCode)
	{
	// Остановка сервиса.
	case SERVICE_CONTROL_STOP:
		ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 0);
		ServiceStop();
		return;

	// Обновление статуса сервиса.
	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}

	ReportStatusToSCMgr(g_ServiceStatus.dwCurrentState, NO_ERROR, 0);
}

/* Устанавливает текущий статус сервиса и сообщает об этом диспетчеру управления сервисами. */
BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;
	BOOL fResult = TRUE;


	if (!bDebug) // при отладке мы не сообщаем в SCM
	{
		if (dwCurrentState == SERVICE_START_PENDING)
			g_ServiceStatus.dwControlsAccepted = 0;
		else
			g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

		g_ServiceStatus.dwCurrentState = dwCurrentState;
		g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
		g_ServiceStatus.dwWaitHint = dwWaitHint;

		if ((dwCurrentState == SERVICE_RUNNING) ||
			(dwCurrentState == SERVICE_STOPPED))
			g_ServiceStatus.dwCheckPoint = 0;
		else
			g_ServiceStatus.dwCheckPoint = dwCheckPoint++;

		// Обновляет информацию о состоянии диспетчера управления службами для вызывающей службы.
		fResult = SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
		if (!fResult)
		{
			AddToMessageLog(TEXT("SetServiceStatus"));
		}
	}
	return fResult;
}

/* Устанавливает сервис. */
void InstallService()
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	TCHAR szPath[512];

	// Получает полный путь к файлу, который содержит указанный модуль. Модуль должен быть загружен текущим процессом.
	if (GetModuleFileName(NULL, szPath, 512) == 0)
	{
		_tprintf(TEXT("Unable to install %s - %s\n"), TEXT(SZSERVICEDISPLAYNAME), GetLastErrorText(sError, 256));
		return;
	}

	// Устанавливает соединение с диспетчером управления службами на указанном компьютере и открывает указанную базу данных диспетчера управления службами.
	schSCManager = OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE  // необходимый доступ: связь с менеджером сервисов, связь с менеджером сервисов;
	);
	if (schSCManager)
	{
		// Создает объект сервиса и добавляет его в указанную базу данных SCM.
		schService = CreateService(
			schSCManager,               // база данных SCManager
			TEXT(SZSERVICENAME),        // название сервиса
			TEXT(SZSERVICEDISPLAYNAME), // отображаемое имя
			SERVICE_QUERY_STATUS,       // необходимый доступ
			SERVICE_WIN32_OWN_PROCESS,  // тип сервиса
			SERVICE_DEMAND_START,       // варианты запуска сервиса
			SERVICE_ERROR_NORMAL,       // тип контроля ошибок
			szPath,                     // полный путь к двоичному файлу сервиса
			NULL,                       // сервис не принадлежит группе
			NULL,                       // существующий тег не меняется
			NULL,                       // у сервиса нет зависимостей
			NULL,                       // использует учетную запись LocalSystem
			NULL);						// нет пароля

		if (schService)
		{
			_tprintf(TEXT("%s installed.\n"), TEXT(SZSERVICEDISPLAYNAME));
			// Закрывает дескриптор объекта сервиса.
			CloseServiceHandle(schService);
		}
		else
		{
			_tprintf(TEXT("CreateService failed - %s\n"), GetLastErrorText(sError, 256));
		}

		// Закрывает дескриптор диспетчера управления сервисами.
		CloseServiceHandle(schSCManager);
	}
	else
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(sError, 256));
}

/* Останавливает и удаляет службу. */
void RemoveService()
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	schSCManager = OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_CONNECT		// необходимый доступ
	);

	if (schSCManager)
	{
		// Открывает существующий сервис.
		schService = OpenService(schSCManager, TEXT(SZSERVICENAME), DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);

		if (schService)
		{
			// Пытается остановить сервис.

			// Отправляет контрольный код в сервис.
			if (ControlService(schService, SERVICE_CONTROL_STOP, &g_ServiceStatus))
			{
				_tprintf(TEXT("Stopping %s."), TEXT(SZSERVICEDISPLAYNAME));
				Sleep(1000);

				//Получает текущий статус указанной службы.
				while (QueryServiceStatus(schService, &g_ServiceStatus))
				{
					if (g_ServiceStatus.dwCurrentState == SERVICE_STOP_PENDING)
					{
						_tprintf(TEXT("."));
						Sleep(1000);
					}
					else
						break;
				}

				if (g_ServiceStatus.dwCurrentState == SERVICE_STOPPED)
					_tprintf(TEXT("\n%s stopped.\n"), TEXT(SZSERVICEDISPLAYNAME));
				else
					_tprintf(TEXT("\n%s failed to stop.\n"), TEXT(SZSERVICEDISPLAYNAME));

			}

			// Удаляет сервис.
			if (DeleteService(schService))
				_tprintf(TEXT("%s removed.\n"), TEXT(SZSERVICEDISPLAYNAME));
			else
				_tprintf(TEXT("DeleteService failed - %s\n"), GetLastErrorText(sError, 256));


			CloseServiceHandle(schService);
		}
		else
			_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(sError, 256));

		CloseServiceHandle(schSCManager);
	}
	else
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(sError, 256));
}

/* Запуск сервиса. */
int RunService()
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	g_StatusHandle = OpenService(hSCManager, SZSERVICENAME, SERVICE_START);
	if (g_StatusHandle)
	{
		if (!StartService(g_StatusHandle, 0, NULL))
		{
			CloseServiceHandle(g_StatusHandle);
			CloseServiceHandle(hSCManager);
			AddToMessageLog("Error: Can't start service");
			_tprintf(TEXT("Error: Can't start service\n"));
			return -1;
		}
		else _tprintf(TEXT("Started successfully\n"));
	}
	else
		_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(sError, 256));
	CloseServiceHandle(g_StatusHandle);
	CloseServiceHandle(hSCManager);
	return 0;
}

/* Остановка сервиса. */
int StopService()
{
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwBytesNeeded;
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!hSCManager)
	{
		AddToMessageLog("Error: Can't open Service Control Manager");
		return -1;
	}
	g_StatusHandle = OpenService(hSCManager, SZSERVICENAME, SERVICE_ALL_ACCESS);
	if (!g_StatusHandle)
	{
		AddToMessageLog("Error: Can't open Service");
		_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(sError, 256));
		return -1;
	}
	// SC_STATUS_PROCESS_INFO - получить информацию о статусе сервиса
	if (!QueryServiceStatusEx(g_StatusHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded))
	{
		AddToMessageLog("Error: QueryServiceStatusEx failed");
		CloseServiceHandle(g_StatusHandle);
		CloseServiceHandle(hSCManager);
		return -1;
	}
	if (ssp.dwCurrentState == SERVICE_STOPPED)
	{
		AddToMessageLog("Service is already stopped");
		_tprintf(TEXT("Service is already stopped\n"));
		CloseServiceHandle(g_StatusHandle);
		CloseServiceHandle(hSCManager);
		return -1;
	}
	if (!ControlService(g_StatusHandle, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&g_ServiceStatus))
	{
		AddToMessageLog("Error: ControlService failed");
		return -1;
	}
	AddToMessageLog("Stopped successfully");
	_tprintf(TEXT("Stopped successfully\n"));
	CloseServiceHandle(g_StatusHandle);
	CloseServiceHandle(hSCManager);
}

/* Запускает сервис как консольное приложение. */
void DebugService(int argc, char ** argv)
{
	DWORD dwArgc;
	LPTSTR *lpszArgv;

#ifdef UNICODE
	lpszArgv = CommandLineToArgvW(GetCommandLineW(), &(dwArgc));
	if (NULL == lpszArgv)
	{
		// CommandLineToArvW failed!!
		_tprintf(TEXT("CmdDebugService CommandLineToArgvW returned NULL\n"));
		return;
	}
#else
	dwArgc = (DWORD)argc;
	lpszArgv = argv;
#endif

	_tprintf(TEXT("Debugging %s.\n"), TEXT(SZSERVICEDISPLAYNAME));

	SetConsoleCtrlHandler(ControlHandler, TRUE);

	ServiceStart(dwArgc, lpszArgv);

#ifdef UNICODE
	// Необходимо освободить память для аргументов

	GlobalFree(lpszArgv);
#endif // UNICODE

}

/* Обработка событий управления консоли. */
BOOL WINAPI ControlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_BREAK_EVENT:  // use Ctrl+C or Ctrl+Break to simulate
	case CTRL_C_EVENT:      // SERVICE_CONTROL_STOP in debug mode
		_tprintf(TEXT("Stopping %s.\n"), TEXT(SZSERVICEDISPLAYNAME));
		ServiceStop();
		return TRUE;
		break;

	}
	return FALSE;
}

/* Позволяет любому потоку записывать сообщения в журнал. */
VOID AddToMessageLog(LPTSTR lpszMsg)
{
	TCHAR szMsg[(sizeof(SZSERVICENAME) / sizeof(TCHAR)) + 100];
	HANDLE  hEventSource;
	LPTSTR  lpszStrings[2];

	if (!bDebug)
	{
		dwErr = GetLastError();

		// Use event logging to log the error.
		//
		hEventSource = RegisterEventSource(NULL, TEXT(SZSERVICENAME));

		_stprintf_s(szMsg, (sizeof(SZSERVICENAME) / sizeof(TCHAR)) + 100, TEXT("%s error: %d"), TEXT(SZSERVICENAME), dwErr);
		lpszStrings[0] = szMsg;
		lpszStrings[1] = lpszMsg;

		if (hEventSource != NULL)
		{
			ReportEvent(hEventSource,		// handle of event source
				EVENTLOG_INFORMATION_TYPE,  // event type
				0,							// event category
				0,							// event ID
				NULL,						// current user's SID
				2,							// strings in lpszStrings
				0,							// no bytes of raw data
				lpszStrings,				// array of error strings
				NULL);						// no raw data

			(VOID)DeregisterEventSource(hEventSource);
		}
	}

	errno_t err;
	FILE *log;
	static BOOL first_launch = TRUE;

	char tmp[1024];
	sprintf(tmp, "%s\\%s", confFilePath, servLog);

	if (first_launch)
	{
		DeleteFileA(tmp);
		first_launch = FALSE;
	}

	if ((err = fopen_s(&log, tmp, "a+")) != 0)
		return;

	fprintf(log, "[log] %s\n", lpszMsg);
	fclose(log);
}

/* Копирует текст сообщения об ошибке в строку. */
LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize)
{
	DWORD dwRet;
	LPTSTR lpszTemp = NULL;

	dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		GetLastError(),
		LANG_NEUTRAL,
		(LPTSTR)&lpszTemp,
		0,
		NULL);

	// поставляемый буфер не достаточно длинный
	if (!dwRet || ((long)dwSize < (long)dwRet + 14))
		lpszBuf[0] = TEXT('\0');
	else
	{
		if (NULL != lpszTemp)
		{
			lpszTemp[lstrlen(lpszTemp) - 2] = TEXT('\0');  //remove cr and newline character
			_stprintf_s(lpszBuf, dwSize, TEXT("%s (0x%x)"), lpszTemp, GetLastError());
		}
	}

	if (NULL != lpszTemp)
		LocalFree((HLOCAL)lpszTemp);

	return lpszBuf;
}

VOID ServiceStart(DWORD dwArgc, LPTSTR *lpszArgv)
{
	DWORD dwWaitStatus;
	HANDLE dwChangeHandle;

	setlocale(LC_ALL, "rus");
	// Инициализация сервиса

	// Сообщает SCM текущий статус сервиса.
	if (!ReportStatusToSCMgr(
		SERVICE_START_PENDING, // состояние сервиса
		NO_ERROR,              // код выхода
		3000))                 // ожидание
		goto cleanup;

	// Создает объект события без имени
	hServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hServiceStopEvent == NULL)
		goto cleanup;

	// ---
	if (configInit() != 0)
	{
		ReportStatusToSCMgr(SERVICE_STOPPED, NO_ERROR, 0);
		goto cleanup;
	}

	// Сообщает SCM текущий статус сервиса.
	if (!ReportStatusToSCMgr(SERVICE_RUNNING, NO_ERROR, 0))
		goto cleanup;

	// Сервис запущен.
	if (createArchive() > 0)
		goto cleanup;

	for (;;)
	{
		// Ожидание, пока объект события не окажется в сигнальном состоянии или пока не истечет время ожидания.
		if (WAIT_OBJECT_0 == WaitForSingleObject(hServiceStopEvent, 0))
			break; // Остановка сервера оповещена.

		// Создает дескриптор уведомлений об изменениях и устанавливает условия фильтра уведомлений об изменениях. 
		dwChangeHandle = FindFirstChangeNotification(
			config.srcPath,					// Полный путь к каталогу для просмотра
			TRUE,							// Функция отслеживает дерево каталогов с корнем в указанном каталоге
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE); // Условия фильтра

		if (dwChangeHandle == INVALID_HANDLE_VALUE || dwChangeHandle == NULL)
		{
			AddToMessageLog("ERROR: FindFirstChangeNotification function failed.");
			break;
		}

		// Проверка уведомлений об изменениях каждую секунду. 
		dwWaitStatus = WaitForSingleObject(dwChangeHandle, 1000);
		switch (dwWaitStatus)
		{
		// Состояние указанного объекта сигнализируется.
		case WAIT_OBJECT_0:
			if (createArchive() > 0)
				break;
			if (FindNextChangeNotification(dwChangeHandle) == FALSE)
			{
				AddToMessageLog("ERROR: FindNextChangeNotification function failed.");
				goto cleanup;
			}
			break;
		// Время ожидания истекло, и состояние объекта не обозначено.
		case WAIT_TIMEOUT:
			break;
		default:
			AddToMessageLog("ERROR: Unhandled dwWaitStatus.");
			goto cleanup;
		}
	}

cleanup:;

}

/*  If a ServiceStop procedure is going to  take longer than 3 seconds to execute,
	it should spawn a thread to execute the stop code, and return. Otherwise, the
	ServiceControlManager will believe that the service has stopped responding.
*/
VOID ServiceStop()
{
	// //Устанавливает указанный объект события в сигнальное состояние.
	if (hServiceStopEvent)
		SetEvent(hServiceStopEvent);
}