#include <Windows.h>
#include <tchar.h>
#include <process.h>
#include <stdio.h>
#include <assert.h>
#include "CompletionPort.h"

static HANDLE completionPort;

static UINT WINAPI ProcessOpers(LPVOID arg);

// associa um conjunto de threads à IOCP
static VOID ThreadPoolCreate() {
	for (int i = 0; i < MAX_THREADS; ++i) {
		_beginthreadex(NULL, 0, ProcessOpers, (LPVOID)completionPort, 0, NULL);
	}
}


BOOL CompletionPortCreate(int maxConcurrency) {
	completionPort= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, maxConcurrency);
	if (completionPort == NULL) return FALSE;
	ThreadPoolCreate();
	return TRUE;
}

VOID CompletionPortClose() {
	CloseHandle(completionPort);
}

BOOL CompletionPortAssociateHandle(HANDLE devHandle, LPVOID completionKey) {
	HANDLE h = CreateIoCompletionPort(devHandle, completionPort, (ULONG_PTR) completionKey, 0);
	return h == completionPort;
}


// função executada pelas threads associadas à IOCP
static UINT WINAPI ProcessOpers(LPVOID arg) {
	HANDLE completionPort = (HANDLE)arg;
	DWORD bytesRead;
	ULONG dummy;
	LPOVERLAPPED ovr;

	//printf("start worker!\n");
	while (TRUE) {
		BOOL res = GetQueuedCompletionStatus(completionPort, &bytesRead, &dummy, &ovr, INFINITE);
		// terminate thread if iocp has been closed
		if (res == false && (GetLastError() == ERROR_ABANDONED_WAIT_0 || GetLastError() == ERROR_INVALID_HANDLE)) {
			break;
		}
		// wait for another io completion if the current io completion return an error different of EOF
		if (res == false && GetLastError() != ERROR_HANDLE_EOF) {
			_tprintf(_T("Error %d getting activity packet!\n"), GetLastError());
			continue;
		}

		if (res == false && GetLastError() == ERROR_HANDLE_EOF)
			SearchSessionProcessNextFile(ovr);
		else
			SearchSessionProcessCurrentFile(ovr, bytesRead);
	 
	}

	return 0;
}
