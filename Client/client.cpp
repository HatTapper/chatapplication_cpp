#include "client.h"

static bool isRunning = true;
static bool shiftIsDown = false;
static HWND hChatPanel = NULL;
static HWND hwndChatDisplay = NULL;
static Client* client = NULL;
WNDPROC g_OldEditProc = NULL;

void ClientTask(Client* client)
{
	while (client->connectedToServer)
	{
		client->PollReceivingMessages(100);
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
	{
		isRunning = false;
		PostQuitMessage(0);
		return 0;
	};
	case WM_CLOSE:
	{
		isRunning = false;
		DestroyWindow(hwnd);
		return 0;
	};
	case WM_SIZE:
	{
		return 0;
	};
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
		EndPaint(hwnd, &ps);
		return 0;
	};
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_KEYDOWN:
	{
		if (wParam == VK_RETURN)
		{
			wchar_t buffer[256];
			GetWindowText(hwnd, buffer, 256);
			SetWindowText(hwnd, L"");
			client->SendMessageToServer(std::wstring(buffer));
			return 0;
		}
		break;
	};
	}
	return CallWindowProc(g_OldEditProc, hwnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		HWND hEdit = GetDlgItem(hwnd, IDC_EDIT1);
		g_OldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);
		return TRUE;
	};
	case WM_CLOSE:
	{
		EndDialog(hwnd, 0);
		return 0;
	};
	}
	return FALSE;
}

void WinHandleMessages()
{
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	WSADATA wsaData;
	FILE *fpOut, *fpIn;
	WNDCLASS wc = {};
	const wchar_t* CLASS_NAME = L"ChatClientWindowClass";
	int freopenRes;

	if (CoInitializeEx(NULL, NULL) != S_OK)
		return 1;

	// creates a console to be used as the logging interface for the server
	if (AllocConsole() == 0)
		return 1;
	if ((freopenRes = freopen_s(&fpOut, "CONOUT$", "w", stdout)) != 0)
		return 1;
	if ((freopenRes = freopen_s(&fpIn, "CONIN$", "r", stdin)) != 0)
		return 1;

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

#define WIDTH 720
#define HEIGHT 540
	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Chat Client",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd == NULL)
		return 0;

	hwndChatDisplay = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
		0, 0, WIDTH, HEIGHT - 120,
		hwnd,
		(HMENU)2,
		hInstance,
		NULL);

	ShowWindow(hwnd, nShowCmd);

	hChatPanel = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_CHATCLIENT_DIALOG), hwnd, DialogProc, NULL);
	UpdateWindow(hwnd);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup failed.\n";
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		std::cout << "Could not find a usable version of Winsock.dll\n";
		WSACleanup();
		return 1;
	}

	client = new Client("user1", "localhost", "25565");
	client->SetMessageTarget(hwndChatDisplay);
	std::thread clientTask(ClientTask, client);

	while (isRunning)
	{
		WinHandleMessages();
	}

	if (client->connectedToServer)
	{
		client->CloseConnectionToServer();
	}
	clientTask.join();

	delete client;
	CoUninitialize();
	WSACleanup();

	return 0;
}
