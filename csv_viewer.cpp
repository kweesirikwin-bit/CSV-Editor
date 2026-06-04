// CSV Editor - Single-file Windows GUI Application
// Features: View CSV files, Edit cells, Save changes
// Compile: g++ csv_viewer.cpp -o csv_viewer.exe -mwindows -static -O2 -lcomctl32 -lcomdlg32 -lgdi32

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// Global variables
HWND g_hListView = NULL;
HWND g_hMainWindow = NULL;
HWND g_hButton = NULL;
HWND g_hButtonSave = NULL;
HWND g_hButtonSaveUTF8 = NULL;
HWND g_hButtonSaveANSI = NULL;
HWND g_hEdit = NULL;
HWND g_hProgressDialog = NULL;
HWND g_hProgressBar = NULL;
HWND g_hProgressText = NULL;
std::vector<std::vector<std::string>> g_csvData;
std::vector<std::vector<bool>> g_wasQuoted; // Track which cells were originally quoted
std::string g_currentFilePath;
bool g_isModified = false;
bool g_cancelLoad = false;
bool g_isWindows874 = false; // Track if file was originally Windows-874
int g_editingRow = -1;
int g_editingCol = -1;
std::string g_originalCellValue; // Store original value for ESC to restore
char g_separator = ','; // Default separator (auto-detected)
WNDPROC g_oldEditProc = NULL; // Original edit control window procedure

// Menu IDs
#define IDM_OPEN 1001
#define IDM_SAVE 1003
#define IDM_EXIT 1002
#define IDM_ABOUT 1004
#define ID_BUTTON_OPEN 2001
#define ID_BUTTON_SAVE 2002
#define ID_BUTTON_SAVE_UTF8 2003
#define ID_BUTTON_SAVE_ANSI 2004
#define ID_EDIT_CELL 3001
#define ID_PROGRESS_BAR 4001
#define ID_PROGRESS_TEXT 4002
#define ID_BUTTON_CANCEL 4003

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ProgressDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateListView(HWND hwndParent);
void CreateButton(HWND hwndParent);
void CreateProgressDialog();
void UpdateProgress(int current, int total, const char* status);
void CloseProgressDialog();
void LoadCSVFile(const char* filename);
void PopulateListView();
void OpenFileDialog();
void SaveCSVFile();
void SaveCSVFileAs(bool saveAsUTF8);
void StartEditCell(int row, int col);
void EndEditCell(bool save);
void UpdateWindowTitle();
void ShowAboutDialog();
HMENU CreateMenuBar();
char DetectSeparator(const std::string& line);
std::vector<std::string> ParseCSVLine(const std::string& line, char separator, std::vector<bool>& wasQuoted);
std::string EscapeCSVField(const std::string& field, char separator, bool forceQuote);
void RemoveBOM(std::string& str);
void CleanHeaderText(std::string& str);
std::string ConvertWindows874ToUTF8(const std::string& input);
std::string ConvertUTF8ToWindows874(const std::string& input);
bool IsLikelyWindows874(const unsigned char* data, size_t length);
std::wstring UTF8ToWide(const std::string& utf8);
std::string WideToUTF8(const std::wstring& wide);

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const char CLASS_NAME[] = "CSVViewerWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_DBLCLKS; // Enable double-click messages

    RegisterClass(&wc);

    // Create menu
    HMENU hMenu = CreateMenuBar();

    // Create window
    g_hMainWindow = CreateWindowEx(
        0,
        CLASS_NAME,
        "CSV Editor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
        NULL,
        hMenu,
        hInstance,
        NULL
    );

    if (g_hMainWindow == NULL) {
        return 0;
    }

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateButton(hwnd);
            CreateListView(hwnd);
            return 0;

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Resize buttons to stay at top
            if (g_hButton) {
                MoveWindow(g_hButton, 10, 10, 120, 30, TRUE);
            }
            if (g_hButtonSaveUTF8) {
                MoveWindow(g_hButtonSaveUTF8, 140, 10, 130, 30, TRUE);
            }

            if (g_hButtonSaveANSI) {
                MoveWindow(g_hButtonSaveANSI, 280, 10, 130, 30, TRUE);
            }

            // Resize ListView to fill remaining space
            if (g_hListView) {
                MoveWindow(g_hListView, 0, 50, rc.right, rc.bottom - 50, TRUE);
            }
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDM_OPEN || LOWORD(wParam) == ID_BUTTON_OPEN) {
                EndEditCell(true); // Save any pending edit
                OpenFileDialog();
                return 0;
            } else if (LOWORD(wParam) == IDM_SAVE || LOWORD(wParam) == ID_BUTTON_SAVE) {
                EndEditCell(true); // Save any pending edit first!
                SaveCSVFile();
                return 0;
            } else if (LOWORD(wParam) == ID_BUTTON_SAVE_UTF8) {
                EndEditCell(true); // Save any pending edit first!
                SaveCSVFileAs(true); // Save as UTF-8
                return 0;
            } else if (LOWORD(wParam) == ID_BUTTON_SAVE_ANSI) {
                EndEditCell(true); // Save any pending edit first!
                SaveCSVFileAs(false); // Save as Windows-874 (ANSI)
                return 0;
            } else if (LOWORD(wParam) == IDM_ABOUT) {
                ShowAboutDialog();
                return 0;
            } else if (LOWORD(wParam) == IDM_EXIT) {
                EndEditCell(true); // Save any pending edit
                PostQuitMessage(0);
                return 0;
            }
            break;

        case WM_NOTIFY: {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            if (pnmhdr->hwndFrom == g_hListView && pnmhdr->code == NM_CLICK) {
                LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)lParam;
                if (pnmia->iItem >= 0 && pnmia->iSubItem >= 0) {
                    StartEditCell(pnmia->iItem, pnmia->iSubItem);
                }
                return 0;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (g_hEdit) {
                    EndEditCell(false); // Cancel edit
                } else {
                    PostQuitMessage(0);
                }
            } else if (wParam == 'O' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                OpenFileDialog();
            } else if (wParam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                SaveCSVFile();
            } else if (wParam == VK_RETURN && g_hEdit) {
                EndEditCell(true); // Save on Enter
            }
            return 0;

        case WM_LBUTTONDBLCLK:
            OpenFileDialog();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Create menu bar
HMENU CreateMenuBar() {
    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    HMENU hHelpMenu = CreateMenu();

    // File menu
    AppendMenu(hFileMenu, MF_STRING, IDM_OPEN, "&Open CSV File...\tCtrl+O");
    AppendMenu(hFileMenu, MF_STRING, IDM_SAVE, "&Save CSV File\tCtrl+S");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, IDM_EXIT, "E&xit");

    // Help menu
    AppendMenu(hHelpMenu, MF_STRING, IDM_ABOUT, "&About CSV Editor...");

    // Add menus to menu bar
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, "&Help");

    return hMenuBar;
}

// Create button
void CreateButton(HWND hwndParent) {
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    // Open button
    g_hButton = CreateWindowEx(
        0,
        "BUTTON",
        "Open CSV",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 10, 120, 30,
        hwndParent,
        (HMENU)ID_BUTTON_OPEN,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Save button (removed - replaced with UTF-8 and ANSI buttons)

    // Save as UTF-8 button
    g_hButtonSaveUTF8 = CreateWindowEx(
        0,
        "BUTTON",
        "Save as UTF-8",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        140, 10, 130, 30,
        hwndParent,
        (HMENU)ID_BUTTON_SAVE_UTF8,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hButtonSaveUTF8, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Save as ANSI (Windows-874) button
    g_hButtonSaveANSI = CreateWindowEx(
        0,
        "BUTTON",
        "Save as ANSI",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        280, 10, 130, 30,
        hwndParent,
        (HMENU)ID_BUTTON_SAVE_ANSI,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hButtonSaveANSI, WM_SETFONT, (WPARAM)hFont, TRUE);

    g_hButtonSave = g_hButtonSaveUTF8; // Keep reference for enabling/disabling
}

// Create ListView control
void CreateListView(HWND hwndParent) {
    g_hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL | WS_HSCROLL | LVS_EDITLABELS,
        0, 50, 0, 0,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // Set extended styles
    ListView_SetExtendedListViewStyle(g_hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Enable Unicode for ListView
    SendMessageW(g_hListView, CCM_SETUNICODEFORMAT, TRUE, 0);

    // Display initial message using Unicode
    LVCOLUMNW lvcNum = {};
    lvcNum.mask = LVCF_TEXT | LVCF_WIDTH;
    lvcNum.pszText = (LPWSTR)L"#";
    lvcNum.cx = 50;
    SendMessageW(g_hListView, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvcNum);

    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = (LPWSTR)L"Welcome to CSV Editor";
    lvc.cx = 850;
    SendMessageW(g_hListView, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);

    LVITEMW lvi = {};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.pszText = (LPWSTR)L"";
    SendMessageW(g_hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

    LVITEMW lviMsg = {};
    lviMsg.iSubItem = 1;
    lviMsg.pszText = (LPWSTR)L"Click 'Open CSV' to load a file. Click any cell to edit it.";
    SendMessageW(g_hListView, LVM_SETITEMTEXTW, 0, (LPARAM)&lviMsg);
}

// Progress dialog window procedure
LRESULT CALLBACK ProgressDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BUTTON_CANCEL) {
                g_cancelLoad = true;
                EnableWindow(GetDlgItem(hwnd, ID_BUTTON_CANCEL), FALSE);
                SetDlgItemText(hwnd, ID_PROGRESS_TEXT, "Cancelling...");
                return 0;
            }
            break;

        case WM_CLOSE:
            g_cancelLoad = true;
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Create progress dialog
void CreateProgressDialog() {
    if (g_hProgressDialog) return;

    // Register progress dialog class
    WNDCLASS wc = {};
    wc.lpfnWndProc = ProgressDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "ProgressDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    // Create dialog window
    g_hProgressDialog = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "ProgressDialogClass",
        "Loading File...",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 400, 150,
        g_hMainWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // Center on parent
    RECT rcParent, rcDialog;
    GetWindowRect(g_hMainWindow, &rcParent);
    GetWindowRect(g_hProgressDialog, &rcDialog);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDialog.right - rcDialog.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDialog.bottom - rcDialog.top)) / 2;
    SetWindowPos(g_hProgressDialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    // Create status text
    g_hProgressText = CreateWindowEx(
        0,
        "STATIC",
        "Please wait...",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 20, 370, 20,
        g_hProgressDialog,
        (HMENU)ID_PROGRESS_TEXT,
        GetModuleHandle(NULL),
        NULL
    );

    // Create progress bar
    g_hProgressBar = CreateWindowEx(
        0,
        PROGRESS_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        10, 50, 370, 25,
        g_hProgressDialog,
        (HMENU)ID_PROGRESS_BAR,
        GetModuleHandle(NULL),
        NULL
    );

    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(g_hProgressBar, PBM_SETSTEP, 1, 0);

    // Create cancel button
    CreateWindowEx(
        0,
        "BUTTON",
        "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 90, 100, 30,
        g_hProgressDialog,
        (HMENU)ID_BUTTON_CANCEL,
        GetModuleHandle(NULL),
        NULL
    );

    ShowWindow(g_hProgressDialog, SW_SHOW);
    UpdateWindow(g_hProgressDialog);
}

// Update progress
void UpdateProgress(int current, int total, const char* status) {
    if (!g_hProgressDialog) return;

    // Update text
    if (g_hProgressText) {
        SetWindowText(g_hProgressText, status);
    }

    // Update progress bar
    if (g_hProgressBar) {
        SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, total));
        SendMessage(g_hProgressBar, PBM_SETPOS, current, 0);
    }

    // Process messages to keep UI responsive
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Close progress dialog
void CloseProgressDialog() {
    if (g_hProgressDialog) {
        DestroyWindow(g_hProgressDialog);
        g_hProgressDialog = NULL;
        g_hProgressBar = NULL;
        g_hProgressText = NULL;
    }
    g_cancelLoad = false;
}

// Remove BOM (Byte Order Mark) from string
void RemoveBOM(std::string& str) {
    // UTF-8 BOM is EF BB BF (3 bytes)
    if (str.length() >= 3) {
        if ((unsigned char)str[0] == 0xEF &&
            (unsigned char)str[1] == 0xBB &&
            (unsigned char)str[2] == 0xBF) {
            str.erase(0, 3);
        }
    }

    // UTF-16 BE BOM is FE FF (2 bytes)
    if (str.length() >= 2) {
        if ((unsigned char)str[0] == 0xFE &&
            (unsigned char)str[1] == 0xFF) {
            str.erase(0, 2);
        }
    }

    // UTF-16 LE BOM is FF FE (2 bytes)
    if (str.length() >= 2) {
        if ((unsigned char)str[0] == 0xFF &&
            (unsigned char)str[1] == 0xFE) {
            str.erase(0, 2);
        }
    }

    // Also remove any other common invisible characters at the start
    while (!str.empty() && (str[0] == '\xEF' || str[0] == '\xBB' || str[0] == '\xBF' ||
           str[0] == '\xFE' || str[0] == '\xFF' || str[0] == '\0')) {
        str.erase(0, 1);
    }
}

// Clean header text by removing invisible and control characters
void CleanHeaderText(std::string& str) {
    // Remove BOM first
    RemoveBOM(str);

    // Remove leading/trailing whitespace and control characters (but NOT high bytes)
    // High bytes (>= 127) are valid in UTF-8 multi-byte sequences
    size_t start = 0;
    while (start < str.length() && str[start] > 0 && str[start] <= 32) {
        start++;
    }

    size_t end = str.length();
    while (end > start && str[end-1] > 0 && str[end-1] <= 32) {
        end--;
    }

    if (start > 0 || end < str.length()) {
        str = str.substr(start, end - start);
    }
}

// Check if data is likely Windows-874 encoded (Thai)
bool IsLikelyWindows874(const unsigned char* data, size_t length) {
    // Check for UTF-8 BOM first
    if (length >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return false; // It's UTF-8 with BOM
    }

    int validUTF8Count = 0;
    int invalidUTF8Count = 0;
    int thaiRangeCount = 0;

    for (size_t i = 0; i < length && i < 1000; i++) {
        unsigned char c = data[i];

        // Count Thai characters in Windows-874: 0xA1-0xDA, 0xDF-0xFB
        if ((c >= 0xA1 && c <= 0xDA) || (c >= 0xDF && c <= 0xFB)) {
            thaiRangeCount++;
        }

        // Check for Thai UTF-8 sequences (Thai Unicode is E0 B8 80 to E0 B9 BF)
        if (c == 0xE0 && i + 2 < length) {
            unsigned char b2 = data[i + 1];
            unsigned char b3 = data[i + 2];

            // Valid Thai UTF-8: E0 B8 80-BF or E0 B9 80-BF
            if ((b2 == 0xB8 || b2 == 0xB9) && (b3 & 0xC0) == 0x80) {
                validUTF8Count++;
                i += 2; // Skip the bytes we just checked
                continue;
            }
        }

        // Check for invalid UTF-8 sequences (starts with continuation byte or invalid starter)
        if ((c & 0xC0) == 0x80) {
            // Continuation byte without starter
            invalidUTF8Count++;
        } else if (c >= 0xC0 && c <= 0xDF && i + 1 < length) {
            // 2-byte sequence
            if ((data[i + 1] & 0xC0) != 0x80) {
                invalidUTF8Count++;
            }
        } else if (c >= 0xE0 && c <= 0xEF && i + 2 < length) {
            // 3-byte sequence (not Thai)
            if ((data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) {
                invalidUTF8Count++;
            }
        }
    }

    // Decision logic:
    // If we have valid Thai UTF-8 sequences, it's UTF-8
    if (validUTF8Count > 0) {
        return false; // UTF-8
    }

    // If we have Thai range bytes and invalid UTF-8 or no UTF-8, it's Windows-874
    if (thaiRangeCount > 0) {
        return true; // Windows-874
    }

    // Default to UTF-8 if no Thai detected
    return false;
}

// Convert Windows-874 (Thai) to UTF-8
std::string ConvertWindows874ToUTF8(const std::string& input) {
    // Windows-874 to Unicode mapping for Thai characters (0x80-0xFF)
    static const unsigned short win874_to_unicode[128] = {
        0x20AC, 0x0081, 0x0082, 0x0083, 0x0084, 0x2026, 0x0086, 0x0087, // 80-87
        0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F, // 88-8F
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
        0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F, // 98-9F
        0x00A0, 0x0E01, 0x0E02, 0x0E03, 0x0E04, 0x0E05, 0x0E06, 0x0E07, // A0-A7
        0x0E08, 0x0E09, 0x0E0A, 0x0E0B, 0x0E0C, 0x0E0D, 0x0E0E, 0x0E0F, // A8-AF
        0x0E10, 0x0E11, 0x0E12, 0x0E13, 0x0E14, 0x0E15, 0x0E16, 0x0E17, // B0-B7
        0x0E18, 0x0E19, 0x0E1A, 0x0E1B, 0x0E1C, 0x0E1D, 0x0E1E, 0x0E1F, // B8-BF
        0x0E20, 0x0E21, 0x0E22, 0x0E23, 0x0E24, 0x0E25, 0x0E26, 0x0E27, // C0-C7
        0x0E28, 0x0E29, 0x0E2A, 0x0E2B, 0x0E2C, 0x0E2D, 0x0E2E, 0x0E2F, // C8-CF
        0x0E30, 0x0E31, 0x0E32, 0x0E33, 0x0E34, 0x0E35, 0x0E36, 0x0E37, // D0-D7
        0x0E38, 0x0E39, 0x0E3A, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x0E3F, // D8-DF
        0x0E40, 0x0E41, 0x0E42, 0x0E43, 0x0E44, 0x0E45, 0x0E46, 0x0E47, // E0-E7
        0x0E48, 0x0E49, 0x0E4A, 0x0E4B, 0x0E4C, 0x0E4D, 0x0E4E, 0x0E4F, // E8-EF
        0x0E50, 0x0E51, 0x0E52, 0x0E53, 0x0E54, 0x0E55, 0x0E56, 0x0E57, // F0-F7
        0x0E58, 0x0E59, 0x0E5A, 0x0E5B, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD  // F8-FF
    };

    std::string output;
    output.reserve(input.length() * 2); // UTF-8 can be up to 3 bytes per Thai char

    for (size_t i = 0; i < input.length(); i++) {
        unsigned char c = input[i];

        if (c < 0x80) {
            // ASCII character - pass through
            output += c;
        } else {
            // High byte - convert using mapping table
            unsigned short unicode = win874_to_unicode[c - 0x80];

            if (unicode == 0xFFFD) {
                // Unmapped character - keep as is
                output += c;
            } else if (unicode < 0x80) {
                // Single byte UTF-8
                output += (char)unicode;
            } else if (unicode < 0x800) {
                // Two byte UTF-8
                output += (char)(0xC0 | (unicode >> 6));
                output += (char)(0x80 | (unicode & 0x3F));
            } else {
                // Three byte UTF-8 (Thai characters are here)
                output += (char)(0xE0 | (unicode >> 12));
                output += (char)(0x80 | ((unicode >> 6) & 0x3F));
                output += (char)(0x80 | (unicode & 0x3F));
            }
        }
    }

    return output;
}

// Convert UTF-8 to Windows-874 (Thai)
std::string ConvertUTF8ToWindows874(const std::string& input) {
    // Unicode to Windows-874 mapping for Thai characters
    // This is reverse of the win874_to_unicode table
    std::string output;
    output.reserve(input.length());

    for (size_t i = 0; i < input.length(); ) {
        unsigned char c = input[i];

        if (c < 0x80) {
            // ASCII - pass through
            output += c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < input.length()) {
            // 2-byte UTF-8 sequence
            unsigned short unicode = ((c & 0x1F) << 6) | (input[i+1] & 0x3F);

            // Check if this maps to Windows-874
            bool found = false;
            if (unicode == 0x20AC) { output += (char)0x80; found = true; } // Euro
            else if (unicode >= 0x2018 && unicode <= 0x2019) {
                output += (char)(0x91 + (unicode - 0x2018)); found = true;
            }
            else if (unicode >= 0x201C && unicode <= 0x201D) {
                output += (char)(0x93 + (unicode - 0x201C)); found = true;
            }
            else if (unicode == 0x2022) { output += (char)0x95; found = true; }
            else if (unicode >= 0x2013 && unicode <= 0x2014) {
                output += (char)(0x96 + (unicode - 0x2013)); found = true;
            }

            if (!found) output += '?'; // Unmappable character
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.length()) {
            // 3-byte UTF-8 sequence (Thai characters are here)
            unsigned short unicode = ((c & 0x0F) << 12) |
                                    ((input[i+1] & 0x3F) << 6) |
                                    (input[i+2] & 0x3F);

            // Thai Unicode range: 0x0E01-0x0E5B maps to Windows-874: 0xA1-0xFB
            if (unicode >= 0x0E01 && unicode <= 0x0E3A) {
                // Thai consonants and vowels: U+0E01-U+0E3A → 0xA1-0xDA
                output += (char)(0xA0 + (unicode - 0x0E00));
            } else if (unicode == 0x0E3F) {
                // Thai Baht sign: U+0E3F → 0xDF
                output += (char)0xDF;
            } else if (unicode >= 0x0E40 && unicode <= 0x0E5B) {
                // Thai vowels and tone marks: U+0E40-U+0E5B → 0xE0-0xFB
                output += (char)(0xA0 + (unicode - 0x0E00));
            } else {
                // Not in Thai range - unmappable
                output += '?';
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < input.length()) {
            // 4-byte UTF-8 - not used in Thai, skip
            output += '?';
            i += 4;
        } else {
            // Invalid UTF-8
            output += '?';
            i++;
        }
    }

    return output;
}

// Convert UTF-8 string to wide string (UTF-16)
std::wstring UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (wideSize == 0) return std::wstring();

    std::wstring wide(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wideSize);

    // Remove null terminator if present
    if (!wide.empty() && wide[wide.length() - 1] == 0) {
        wide.resize(wide.length() - 1);
    }

    return wide;
}

// Convert wide string (UTF-16) to UTF-8 string
std::string WideToUTF8(const std::wstring& wide) {
    if (wide.empty()) return std::string();

    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8Size == 0) return std::string();

    std::string utf8(utf8Size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Size, NULL, NULL);

    // Remove null terminator if present
    if (!utf8.empty() && utf8[utf8.length() - 1] == 0) {
        utf8.resize(utf8.length() - 1);
    }

    return utf8;
}

// Detect separator from a line (comma or pipe)
char DetectSeparator(const std::string& line) {
    int commaCount = 0;
    int pipeCount = 0;
    int tabCount = 0;
    int semicolonCount = 0;
    bool inQuotes = false;

    // Count separators outside of quotes
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];

        if (c == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                i++; // Skip escaped quote
            } else {
                inQuotes = !inQuotes;
            }
        } else if (!inQuotes) {
            if (c == ',') commaCount++;
            else if (c == '|') pipeCount++;
            else if (c == '\t') tabCount++;
            else if (c == ';') semicolonCount++;
        }
    }

    // Return the most common separator
    if (pipeCount > commaCount && pipeCount > tabCount && pipeCount > semicolonCount) {
        return '|';
    } else if (tabCount > commaCount && tabCount > pipeCount && tabCount > semicolonCount) {
        return '\t';
    } else if (semicolonCount > commaCount && semicolonCount > pipeCount && semicolonCount > tabCount) {
        return ';';
    }

    return ','; // Default to comma
}

// Parse a single CSV line with specified separator and track quoted fields
std::vector<std::string> ParseCSVLine(const std::string& line, char separator, std::vector<bool>& wasQuoted) {
    std::vector<std::string> result;
    wasQuoted.clear();

    std::string cell;
    bool inQuotes = false;
    bool cellStartedWithQuote = false;
    bool isFirstChar = true;

    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];

        if (c == '"') {
            if (isFirstChar) {
                cellStartedWithQuote = true;
                isFirstChar = false;
            }

            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                cell += '"';
                i++;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == separator && !inQuotes) {
            result.push_back(cell);
            wasQuoted.push_back(cellStartedWithQuote);
            cell.clear();
            cellStartedWithQuote = false;
            isFirstChar = true;
        } else {
            cell += c;
            isFirstChar = false;
        }
    }
    result.push_back(cell);
    wasQuoted.push_back(cellStartedWithQuote);
    return result;
}

// Load CSV file
void LoadCSVFile(const char* filename) {
    g_csvData.clear();
    g_wasQuoted.clear();
    g_cancelLoad = false;

    // First, read file as binary to detect encoding
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        MessageBox(g_hMainWindow, "Failed to open file!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Get file size for progress calculation
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    bool showProgress = fileSize > (100 * 1024); // Show progress for files > 100KB

    if (showProgress) {
        CreateProgressDialog();
        UpdateProgress(0, 100, "Reading file...");
    }

    // Read first 1000 bytes to detect encoding
    char detectBuffer[1000];
    size_t detectSize = (fileSize < 1000) ? (size_t)fileSize : 1000;
    file.read(detectBuffer, detectSize);
    file.seekg(0, std::ios::beg); // Reset to beginning

    // Detect if file is Windows-874 (Thai encoding)
    bool isWindows874 = IsLikelyWindows874((unsigned char*)detectBuffer, detectSize);

    // Store the original encoding for saving later
    g_isWindows874 = isWindows874;

    // Debug: Show detected encoding
    char encodingMsg[256];
    sprintf(encodingMsg, "Detected encoding: %s", isWindows874 ? "Windows-874 (ANSI)" : "UTF-8");
    if (showProgress) {
        UpdateProgress(0, 100, encodingMsg);
    } else {
        // Show message box for small files
        MessageBox(g_hMainWindow, encodingMsg, "Encoding Detection", MB_OK | MB_ICONINFORMATION);
    }

    if (showProgress && isWindows874) {
        UpdateProgress(0, 100, "Converting from Windows-874 to UTF-8...");
    }

    std::vector<std::string> allLines;
    std::string line;
    std::streampos bytesRead = 0;
    int lastPercent = 0;
    bool firstLine = true;

    // Read all lines with progress
    while (std::getline(file, line)) {
        if (g_cancelLoad) {
            file.close();
            CloseProgressDialog();
            MessageBox(g_hMainWindow, "File loading cancelled!", "Cancelled", MB_OK | MB_ICONINFORMATION);
            return;
        }

        if (!line.empty()) {
            // Remove carriage return if present
            if (line.back() == '\r') {
                line.pop_back();
            }

            // Remove BOM from first line
            if (firstLine) {
                RemoveBOM(line);
                firstLine = false;
            }

            // Convert from Windows-874 to UTF-8 if needed
            if (isWindows874) {
                line = ConvertWindows874ToUTF8(line);
            }

            allLines.push_back(line);
        }

        // Update progress
        if (showProgress) {
            bytesRead = file.tellg();
            int percent = (int)((bytesRead * 100) / fileSize);
            if (percent != lastPercent) {
                char status[128];
                sprintf(status, "Reading file... %d%%", percent);
                UpdateProgress(percent, 100, status);
                lastPercent = percent;
            }
        }
    }

    file.close();

    if (allLines.empty()) {
        CloseProgressDialog();
        MessageBox(g_hMainWindow, "CSV file is empty!", "Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    // Detect separator from first line
    if (showProgress) {
        UpdateProgress(0, 100, "Detecting separator...");
    }
    g_separator = DetectSeparator(allLines[0]);

    // Parse all lines with detected separator
    if (showProgress) {
        UpdateProgress(0, (int)allLines.size(), "Parsing rows...");
    }

    for (size_t i = 0; i < allLines.size(); i++) {
        if (g_cancelLoad) {
            CloseProgressDialog();
            MessageBox(g_hMainWindow, "File loading cancelled!", "Cancelled", MB_OK | MB_ICONINFORMATION);
            return;
        }

        std::vector<bool> rowQuoted;
        g_csvData.push_back(ParseCSVLine(allLines[i], g_separator, rowQuoted));
        g_wasQuoted.push_back(rowQuoted);

        // Update progress every 100 rows
        if (showProgress && (i % 100 == 0 || i == allLines.size() - 1)) {
            char status[128];
            sprintf(status, "Parsing rows... %zu/%zu", i + 1, allLines.size());
            UpdateProgress((int)(i + 1), (int)allLines.size(), status);
        }
    }

    // Populate the list view
    if (showProgress) {
        UpdateProgress(0, 100, "Populating table...");
    }

    g_currentFilePath = filename;
    g_isModified = false;
    EnableWindow(g_hButtonSave, TRUE);
    PopulateListView();
    UpdateWindowTitle();

    CloseProgressDialog();
}

// Populate ListView with CSV data
void PopulateListView() {
    if (g_csvData.empty()) return;

    // Clear existing columns and items
    ListView_DeleteAllItems(g_hListView);
    while (ListView_DeleteColumn(g_hListView, 0));

    // Get column count from first row
    size_t colCount = g_csvData[0].size();

    // Add row number column first (Unicode)
    LVCOLUMNW lvcRow = {};
    lvcRow.mask = LVCF_TEXT | LVCF_WIDTH;
    lvcRow.pszText = (LPWSTR)L"#";
    lvcRow.cx = 50; // Narrow column for row numbers
    SendMessageW(g_hListView, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvcRow);

    // Add data columns (offset by 1 because of row number column) using Unicode
    for (size_t i = 0; i < colCount; i++) {
        // Clean the header text to remove BOM and special characters
        std::string cleanHeader = g_csvData[0][i];
        CleanHeaderText(cleanHeader);

        // Convert to wide string for proper Thai display
        std::wstring wideHeader = UTF8ToWide(cleanHeader);

        LVCOLUMNW lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = (LPWSTR)wideHeader.c_str();
        lvc.cx = 150; // Default column width
        SendMessageW(g_hListView, LVM_INSERTCOLUMNW, i + 1, (LPARAM)&lvc);
    }

    // Add data rows (skip first row as it's the header)
    for (size_t row = 1; row < g_csvData.size(); row++) {
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row - 1;
        lvi.iSubItem = 0;

        // Row number in first column
        wchar_t rowNum[16];
        swprintf(rowNum, 16, L"%zu", row);
        lvi.pszText = rowNum;
        SendMessageW(g_hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

        // Add actual data columns (offset by 1) using Unicode
        if (!g_csvData[row].empty()) {
            for (size_t col = 0; col < g_csvData[row].size() && col < colCount; col++) {
                // Convert UTF-8 to wide string for proper Thai display
                std::wstring wideText = UTF8ToWide(g_csvData[row][col]);

                LVITEMW lviSub = {};
                lviSub.iSubItem = col + 1;
                lviSub.pszText = (LPWSTR)wideText.c_str();
                SendMessageW(g_hListView, LVM_SETITEMTEXTW, row - 1, (LPARAM)&lviSub);
            }
        }
    }

    UpdateWindowTitle();
}

// Open file dialog
void OpenFileDialog() {
    char filename[MAX_PATH] = "";

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hMainWindow;
    ofn.lpstrFilter = "All Delimited Files\0*.csv;*.psv;*.tsv;*.txt\0CSV Files (*.csv)\0*.csv\0Pipe-Separated (*.psv)\0*.psv\0Tab-Separated (*.tsv)\0*.tsv\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "csv";
    ofn.lpstrTitle = "Open Delimited File (CSV/PSV/TSV)";

    if (GetOpenFileName(&ofn)) {
        LoadCSVFile(filename);
    }
}

// Escape CSV field - preserve original quote formatting
std::string EscapeCSVField(const std::string& field, char separator, bool wasQuoted) {
    // Check if field needs quotes
    bool needsQuotes = wasQuoted; // Keep original quote status

    if (!needsQuotes) {
        // Also add quotes if field contains separator, quote, or newline
        for (char c : field) {
            if (c == separator || c == '"' || c == '\n' || c == '\r') {
                needsQuotes = true;
                break;
            }
        }
    }

    if (!needsQuotes) {
        return field; // Return unquoted
    }

    // Add quotes and escape any internal quotes by doubling them
    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";  // Double the quote
        } else {
            escaped += c;
        }
    }
    escaped += "\"";

    return escaped;
}

// Save CSV file
void SaveCSVFile() {
    if (g_currentFilePath.empty()) {
        MessageBox(g_hMainWindow, "No file is currently loaded!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    if (g_csvData.empty()) {
        MessageBox(g_hMainWindow, "No data to save!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::ofstream file(g_currentFilePath, std::ios::binary);

    if (!file.is_open()) {
        char msg[512];
        sprintf(msg, "Failed to save file:\n%s", g_currentFilePath.c_str());
        MessageBox(g_hMainWindow, msg, "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Write all rows using detected separator and preserve quote formatting
    for (size_t row = 0; row < g_csvData.size(); row++) {
        for (size_t col = 0; col < g_csvData[row].size(); col++) {
            bool wasQuoted = false;

            // Check if this cell was originally quoted
            if (row < g_wasQuoted.size() && col < g_wasQuoted[row].size()) {
                wasQuoted = g_wasQuoted[row][col];
            }

            std::string fieldData = EscapeCSVField(g_csvData[row][col], g_separator, wasQuoted);

            // Convert back to Windows-874 if original file was Windows-874
            if (g_isWindows874) {
                fieldData = ConvertUTF8ToWindows874(fieldData);
            }

            file << fieldData;
            if (col < g_csvData[row].size() - 1) {
                file << g_separator;
            }
        }
        file << "\r\n"; // Windows line ending
    }

    file.close();

    // Verify the file was written
    if (!file.good()) {
        MessageBox(g_hMainWindow, "Error occurred while writing file!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    g_isModified = false;
    UpdateWindowTitle();

    // Show success with details
    char msg[512];
    const char* encoding = g_isWindows874 ? "Windows-874" : "UTF-8";
    sprintf(msg, "Successfully saved %zu rows to:\n%s\n\nEncoding: %s",
            g_csvData.size(), g_currentFilePath.c_str(), encoding);
    MessageBox(g_hMainWindow, msg, "Success", MB_OK | MB_ICONINFORMATION);
}

// Save CSV file with specific encoding
void SaveCSVFileAs(bool saveAsUTF8) {
    if (g_currentFilePath.empty()) {
        MessageBox(g_hMainWindow, "No file is currently loaded!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    if (g_csvData.empty()) {
        MessageBox(g_hMainWindow, "No data to save!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::ofstream file(g_currentFilePath, std::ios::binary);

    if (!file.is_open()) {
        char msg[512];
        sprintf(msg, "Failed to save file:\n%s", g_currentFilePath.c_str());
        MessageBox(g_hMainWindow, msg, "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Write all rows using detected separator and preserve quote formatting
    for (size_t row = 0; row < g_csvData.size(); row++) {
        for (size_t col = 0; col < g_csvData[row].size(); col++) {
            bool wasQuoted = false;

            // Check if this cell was originally quoted
            if (row < g_wasQuoted.size() && col < g_wasQuoted[row].size()) {
                wasQuoted = g_wasQuoted[row][col];
            }

            std::string fieldData = EscapeCSVField(g_csvData[row][col], g_separator, wasQuoted);

            // Convert to specified encoding
            if (!saveAsUTF8) {
                // Save as Windows-874 (ANSI)
                fieldData = ConvertUTF8ToWindows874(fieldData);
            }
            // If saveAsUTF8 is true, keep as UTF-8 (no conversion needed)

            file << fieldData;
            if (col < g_csvData[row].size() - 1) {
                file << g_separator;
            }
        }
        file << "\r\n"; // Windows line ending
    }

    file.close();

    // Verify the file was written
    if (!file.good()) {
        MessageBox(g_hMainWindow, "Error occurred while writing file!", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Update the original encoding flag
    g_isWindows874 = !saveAsUTF8;

    g_isModified = false;
    UpdateWindowTitle();

    // Show success with details
    char msg[512];
    const char* encoding = saveAsUTF8 ? "UTF-8" : "Windows-874 (ANSI)";
    sprintf(msg, "Successfully saved %zu rows to:\n%s\n\nEncoding: %s",
            g_csvData.size(), g_currentFilePath.c_str(), encoding);
    MessageBox(g_hMainWindow, msg, "Success", MB_OK | MB_ICONINFORMATION);
}

// Update window title
void UpdateWindowTitle() {
    if (g_csvData.empty()) {
        SetWindowText(g_hMainWindow, "CSV Editor");
        return;
    }

    char title[512];
    const char* filename = strrchr(g_currentFilePath.c_str(), '\\');
    filename = filename ? filename + 1 : g_currentFilePath.c_str();

    const char* sepName = (g_separator == ',') ? "CSV" :
                          (g_separator == '|') ? "PSV" :
                          (g_separator == '\t') ? "TSV" :
                          (g_separator == ';') ? "SSV" : "CSV";

    sprintf(title, "%s%s - %zu rows, %zu cols [%s] - CSV Editor",
            filename,
            g_isModified ? " *" : "",
            g_csvData.size() - 1,
            g_csvData.empty() ? 0 : g_csvData[0].size(),
            sepName);

    SetWindowText(g_hMainWindow, title);
}

// Show About dialog
void ShowAboutDialog() {
    MessageBox(g_hMainWindow,
               "CSV Editor\n\n"
               "Kwee Sirikwin\n"
               "SCB TechX\n"
               "2026",
               "About CSV Editor",
               MB_OK | MB_ICONINFORMATION);
}

// Subclass window procedure for edit control to handle ESC and ENTER keys
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            EndEditCell(true); // Save on Enter
            return 0;
        } else if (wParam == VK_ESCAPE) {
            EndEditCell(false); // Discard on ESC
            return 0;
        }
    }

    // Call original window procedure for all other messages
    return CallWindowProcW(g_oldEditProc, hwnd, uMsg, wParam, lParam);
}

// Start editing a cell
void StartEditCell(int row, int col) {
    if (g_csvData.empty() || row < 0 || col < 0) return;

    // Don't allow editing the row number column (column 0)
    if (col == 0) return;

    // Adjust column index (subtract 1 because first column is row number)
    int dataCol = col - 1;

    // Actual data row is row + 1 (because first row is header)
    int dataRow = row + 1;
    if (dataRow >= (int)g_csvData.size() || dataCol >= (int)g_csvData[dataRow].size()) return;

    // End previous edit if any
    EndEditCell(true);

    // Get cell rectangle for the clicked column (col includes row number column)
    RECT rcItem;

    // Get subitem rectangle (col already points to the display column)
    ListView_GetSubItemRect(g_hListView, row, col, LVIR_BOUNDS, &rcItem);

    // Adjust for proper alignment
    rcItem.left += 2;
    rcItem.right -= 2;

    // Ensure minimum height
    if ((rcItem.bottom - rcItem.top) < 20) {
        rcItem.bottom = rcItem.top + 20;
    }

    // Create edit control
    if (g_hEdit) {
        DestroyWindow(g_hEdit);
    }

    g_hEdit = CreateWindowExW(
        0,
        L"EDIT",
        L"",  // Set text later with SetWindowTextW for Unicode support
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        rcItem.left, rcItem.top,
        rcItem.right - rcItem.left, rcItem.bottom - rcItem.top,
        g_hListView,
        (HMENU)ID_EDIT_CELL,
        GetModuleHandle(NULL),
        NULL
    );

    // Subclass the edit control to handle ESC and ENTER keys
    g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    // Set font
    HFONT hFont = (HFONT)SendMessage(g_hListView, WM_GETFONT, 0, 0);
    SendMessage(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Store original value for ESC to restore
    g_originalCellValue = g_csvData[dataRow][dataCol];

    // Set text using Unicode API for proper Thai support
    std::wstring wideText = UTF8ToWide(g_csvData[dataRow][dataCol]);
    SetWindowTextW(g_hEdit, wideText.c_str());

    // Set background color to make it more visible
    SendMessage(g_hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(2, 2));

    // Select all text and focus
    SendMessage(g_hEdit, EM_SETSEL, 0, -1);
    SetFocus(g_hEdit);

    g_editingRow = dataRow;
    g_editingCol = dataCol;
}

// End editing a cell
void EndEditCell(bool save) {
    if (!g_hEdit || g_editingRow < 0 || g_editingCol < 0) return;

    if (save) {
        // Get text from edit control using Unicode API for Thai support
        int len = GetWindowTextLengthW(g_hEdit);
        wchar_t* wideBuffer = new wchar_t[len + 1];
        wideBuffer[0] = L'\0';

        if (len > 0) {
            GetWindowTextW(g_hEdit, wideBuffer, len + 1);
        }

        // Convert wide string to UTF-8
        std::wstring wideText(wideBuffer);
        std::string newValue = WideToUTF8(wideText);

        // Update data - always update even if empty
        if (g_editingRow < (int)g_csvData.size() &&
            g_editingCol < (int)g_csvData[g_editingRow].size()) {

            // Check if value actually changed
            if (g_csvData[g_editingRow][g_editingCol] != newValue) {
                g_csvData[g_editingRow][g_editingCol] = newValue;
                g_isModified = true;
                UpdateWindowTitle();

                // Update ListView display (add 1 to column for row number column)
                // Use Unicode for proper Thai display
                std::wstring wideDisplay = UTF8ToWide(newValue);
                LVITEMW lviUpdate = {};
                lviUpdate.iSubItem = g_editingCol + 1;
                lviUpdate.pszText = (LPWSTR)wideDisplay.c_str();
                SendMessageW(g_hListView, LVM_SETITEMTEXTW, g_editingRow - 1, (LPARAM)&lviUpdate);

                // Force refresh
                InvalidateRect(g_hListView, NULL, TRUE);
                UpdateWindow(g_hListView);
            }
        }

        delete[] wideBuffer;
    } else {
        // ESC pressed - restore original value
        if (g_editingRow < (int)g_csvData.size() &&
            g_editingCol < (int)g_csvData[g_editingRow].size()) {

            // Restore original value in data (in case it was changed)
            g_csvData[g_editingRow][g_editingCol] = g_originalCellValue;

            // Update ListView display to show original value
            std::wstring wideDisplay = UTF8ToWide(g_originalCellValue);
            LVITEMW lviUpdate = {};
            lviUpdate.iSubItem = g_editingCol + 1;
            lviUpdate.pszText = (LPWSTR)wideDisplay.c_str();
            SendMessageW(g_hListView, LVM_SETITEMTEXTW, g_editingRow - 1, (LPARAM)&lviUpdate);

            // Force refresh
            InvalidateRect(g_hListView, NULL, TRUE);
            UpdateWindow(g_hListView);
        }
    }

    // Clean up
    if (g_hEdit) {
        DestroyWindow(g_hEdit);
        g_hEdit = NULL;
    }
    g_editingRow = -1;
    g_editingCol = -1;
    g_originalCellValue.clear(); // Clear the stored original value

    // Return focus to ListView
    if (g_hListView) {
        SetFocus(g_hListView);
    }
}
