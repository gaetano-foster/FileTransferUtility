#define _WIN32_WINNT			0x0600
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <CommCtrl.h> 
#include <Shlwapi.h>
#include <stdio.h>
#define MAX_RECURSE_DEPTH		5

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI
WinMain(HINSTANCE hInstance,
		HINSTANCE hPrevInstance,
		LPSTR args,
		int ncmdshow)
{
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_STANDARD_CLASSES; // could also use ICC_WIN95_CLASSES
	InitCommonControlsEx(&icex);

	const wchar_t *class_name = L"MyWindowClass";
	WNDCLASSEX wc;

	wc.cbSize			= sizeof(WNDCLASSEX);                // The size, in bytes, of this structure
	wc.style			= 0;                                 // The class style(s)
	wc.lpfnWndProc		= WindowProc;                        // A pointer to the window procedure
	wc.cbClsExtra		= 0;                                 // The number of extra bytes to allocate following the window-class structure.
	wc.cbWndExtra		= 0;                                 // The number of extra bytes to allocate following the window instance.
	wc.hInstance		= hInstance;                         // A handle to the instance that contains the window procedure for the class.
	wc.hIcon			= LoadIcon(NULL, IDI_APPLICATION);   // A handle to the class icon. This member must be a handle to an icon resource. If this member is NULL, the system provides a default icon.
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);       // A handle to the class cursor. This member must be a handle to a cursor resource. If this member is NULL, an application must explicitly set the cursor shape whenever the mouse moves into the application's window.
	wc.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);          // A handle to the class background brush.
	wc.lpszMenuName		= NULL;                              // Pointer to a null-terminated character string that specifies the resource name of the class menu.
	wc.lpszClassName	= class_name;						 // A string that identifies the window class.
	wc.hIconSm			= LoadIcon(NULL, IDI_APPLICATION);   // A handle to a small icon that is associated with the window class.Create the window

	if(!RegisterClassEx(&wc)) {
		MessageBox(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}
	HWND hwnd;
	hwnd = CreateWindowEx(
		0,                      // Optional window styles.
		class_name,				// Window class
		L"File Transfer Utility",            // Window text
		WS_OVERLAPPEDWINDOW,    // Window style
		CW_USEDEFAULT,          // Position X
		CW_USEDEFAULT,          // Position Y
		500,                    // Width
		400,                    // Height
		NULL,                   // Parent window
		NULL,                   // Menu
		hInstance,              // Instance handle
		NULL                    // Additional application data
	);

	if(hwnd == NULL) {
		MessageBox(NULL, L"Create Window Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}
	ShowWindow(hwnd, ncmdshow);
	UpdateWindow(hwnd); 

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void 
LogWin32Error(FILE *log, 
	const wchar_t *context)
{
	DWORD err = GetLastError();
	wchar_t *msg = NULL;

	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err, 0, (LPWSTR)&msg, 0, NULL);

	if (msg) {
		fwprintf(log, L"%s: %s", context, msg);
		LocalFree(msg);
	} else {
		fwprintf(log, L"%s: Unknown error (%lu)\n", context, err);
	}
}

typedef struct {
	const wchar_t *toDir;
	int action;
	FILE *log;
	const wchar_t **targets;
	int targetCount;
	BOOL preserveStructure;
	const wchar_t *fromDir;
} SearchContext;

HWND hwndFromDir;
HWND hwndToDir;

wchar_t fromDirectoryBuffer[512];
wchar_t toDirectoryBuffer[512];

BOOL preserveStructure = FALSE;

void 
SearchAndProcess(const wchar_t *currentDir,
				 int depth,
				 SearchContext *ctx)
{
	if (depth > MAX_RECURSE_DEPTH)
		return;

	wchar_t searchPath[MAX_PATH];
	swprintf(searchPath, MAX_PATH, L"%s\\*", currentDir);

	WIN32_FIND_DATAW ffd;
	HANDLE hFind = FindFirstFileW(searchPath, &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do {
		if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
			continue;

		wchar_t fullPath[MAX_PATH];
		swprintf(fullPath, MAX_PATH, L"%s\\%s", currentDir, ffd.cFileName);

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			SearchAndProcess(fullPath, depth + 1, ctx);
		} else {
			for (int i = 0; i < ctx->targetCount; i++) {
				if (PathMatchSpecW(ffd.cFileName, ctx->targets[i])) {
					// match found
					wchar_t dstPath[MAX_PATH];

					if (ctx->preserveStructure) {
						wchar_t relPath[MAX_PATH];
						PathRelativePathToW(relPath, ctx->fromDir, FILE_ATTRIBUTE_DIRECTORY, fullPath, FILE_ATTRIBUTE_NORMAL);
						PathRemoveBackslashW(relPath); // optional

						swprintf(dstPath, MAX_PATH, L"%s\\%s", ctx->toDir, relPath + 2); // +2 skips ".\"

						wchar_t folderOnly[MAX_PATH];
						wcscpy(folderOnly, dstPath);
						PathRemoveFileSpecW(folderOnly);
						CreateDirectoryW(folderOnly, NULL);
					} else {
						swprintf(dstPath, MAX_PATH, L"%s\\%s", ctx->toDir, ffd.cFileName);
					}
					BOOL success = FALSE;

					if (ctx->action == 0)
						success = MoveFileW(fullPath, dstPath);
					else if (ctx->action == 1)
						success = CopyFileW(fullPath, dstPath, FALSE);
					else if (ctx->action == 2)
						success = DeleteFileW(fullPath);

					if (!success) {
						fwprintf(ctx->log, L"Failed to %s: %s\n",
							(ctx->action == 0) ? L"move" :
							(ctx->action == 1) ? L"copy" : L"delete",
							fullPath);
						LogWin32Error(ctx->log, L"File operation");
					}
				}
			}
		}
	} while (FindNextFileW(hFind, &ffd));

	FindClose(hFind);
}

BOOL 
ProcessListFile(const wchar_t *fromDir,
				const wchar_t *toDir,
				int action)
{
	FILE *lists = _wfopen(L"FileUtilsLists.txt", L"r, ccs=UTF-8");
	if (!lists) {
		MessageBox(NULL, L"Could not open FileUtilsLists.txt", L"Error", MB_ICONERROR);
		  return FALSE;
	}

	FILE *logs = _wfopen(L"FileUtilsLogs.txt", L"a, ccs=UTF-8");
	if (!logs) {
		MessageBox(NULL, L"Could not open FileUtilsLogs.txt", L"Error", MB_ICONERROR);
		fclose(lists);
		return FALSE;
	}

	const wchar_t *filenames[1000];
	int count = 0;

	static wchar_t lineBufs[1000][512]; // avoid malloc, use static buffers
	while (count < 1000 && fgetws(lineBufs[count], 512, lists)) {
		wchar_t *newline = wcspbrk(lineBufs[count], L"\r\n");
		if (newline) *newline = 0;
		if (wcslen(lineBufs[count]) > 0) {
			filenames[count] = lineBufs[count];
			count++;
		}
	}

	SearchContext ctx = {
		.toDir = toDir,
		.action = action,
		.log = logs,
		.targets = filenames,
		.targetCount = count,
		.preserveStructure = preserveStructure,
		.fromDir = fromDir
	};

	SearchAndProcess(fromDir, 0, &ctx);

	fclose(lists);
	fclose(logs);
	return TRUE;
}

LRESULT CALLBACK 
WindowProc(HWND hwnd,
		   UINT uMsg,
		   WPARAM wParam,
		   LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		hwndFromDir = CreateWindowEx(
			WS_EX_CLIENTEDGE,
			WC_EDITW, NULL,
			WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
			50, 50, 380, 25,
			hwnd, (HMENU)1001,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);

		HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		SendMessage(hwndFromDir, WM_SETFONT, (WPARAM)hFont, TRUE);
		CreateWindowW(
			L"STATIC",						// class name
			L"From Directory:",				// label text
			WS_CHILD | WS_VISIBLE,			// styles
			50, 30, 300, 20,				// x, y, width, height
			hwnd, NULL,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);

		hwndToDir = CreateWindowEx(
			WS_EX_CLIENTEDGE,
			WC_EDITW, NULL,
			WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
			50, 100, 380, 25,
			hwnd, (HMENU)1001,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);
		SendMessage(hwndToDir, WM_SETFONT, (WPARAM)hFont, TRUE);
		CreateWindowW(
			L"STATIC",						// class name
			L"To Directory:",				// label text
			WS_CHILD | WS_VISIBLE,			// styles
			50, 80, 300, 20,				// x, y, width, height
			hwnd, NULL,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);

		HWND hwndMoveButton = CreateWindowW(
			L"BUTTON", L"Move",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			50, 150, 80, 30,
			hwnd, (HMENU)1002,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);

		HWND hwndCopyButton = CreateWindowW(
			L"BUTTON", L"Copy",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			140, 150, 80, 30,
			hwnd, (HMENU)1003,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);

		HWND hwndDeleteButton = CreateWindowW(
			L"BUTTON", L"Delete",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			230, 150, 80, 30,
			hwnd, (HMENU)1003,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);

		HWND hwndPreserveStructure = CreateWindowW(
			L"BUTTON", L"Preserve folder structure",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
			50, 200, 250, 20,
			hwnd, (HMENU)1005,
			(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
			NULL);
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case 1002: // Move Directory button
			GetWindowText(hwndFromDir, fromDirectoryBuffer, 512);
			GetWindowText(hwndToDir, toDirectoryBuffer, 512);
			preserveStructure = (IsDlgButtonChecked(hwnd, 1005) == BST_CHECKED);
			if (!ProcessListFile(fromDirectoryBuffer, toDirectoryBuffer, 0)) return 0;
			MessageBox(hwnd, L"Moved Files!", "", MB_OK); // just to confirm
			break;
		case 1003: // Copy Directory button
			GetWindowText(hwndFromDir, fromDirectoryBuffer, 512);
			GetWindowText(hwndToDir, toDirectoryBuffer, 512);
			preserveStructure = (IsDlgButtonChecked(hwnd, 1005) == BST_CHECKED);
			if (!ProcessListFile(fromDirectoryBuffer, toDirectoryBuffer, 1)) return 0;
			MessageBox(hwnd, L"Copied Files!", "", MB_OK); // just to confirm
			break;
		case 1004: // Delete Directory button
			GetWindowText(hwndFromDir, fromDirectoryBuffer, 512);
			GetWindowText(hwndToDir, toDirectoryBuffer, 512);
			preserveStructure = (IsDlgButtonChecked(hwnd, 1005) == BST_CHECKED);
			if (!ProcessListFile(fromDirectoryBuffer, toDirectoryBuffer, 2)) return 0;
			MessageBox(hwnd, L"Deleted Files!", "", MB_OK); // just to confirm
			break;
		}
	break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}