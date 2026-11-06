#include "../Comum/header.h"
#include "resource.h"
#include <stdio.h>

#define CLASSE_JANELA _T("MonitorSO2Classe")

#define WM_APP_ATUALIZAR  (WM_APP + 1)
#define WM_APP_ENCERRAR   (WM_APP + 2)

#define COR_FUNDO          RGB(241, 245, 249)
#define COR_BANNER         RGB(15, 52, 96)
#define COR_BANNER_TEXTO   RGB(255, 255, 255)
#define COR_CAB_TABELA     RGB(226, 232, 240)
#define COR_CAB_TABELA_TX  RGB(51, 65, 85)
#define COR_LINHA_PAR      RGB(255, 255, 255)
#define COR_LINHA_IMPAR    RGB(248, 250, 252)
#define COR_BORDA          RGB(203, 213, 225)
#define COR_TEXTO          RGB(30, 41, 59)
#define COR_SUBTEXTO       RGB(100, 116, 139)
#define COR_DESTAQUE       RGB(234, 88, 12)
#define COR_VAZIO          RGB(148, 163, 184)

#define ALT_BANNER         64
#define ALT_CAB_COLUNAS    34
#define ALT_LINHA          52
#define ALT_RODAPE         40
#define MARGEM             16

typedef struct {
    DWORD identificador;
    DWORD duracao;
    TCHAR msg[140];
} ALERTA_VISIVEL;

static HINSTANCE g_hInst = NULL;
static HWND g_hWnd = NULL;
static HANDLE g_hThreadSHM = NULL;
static volatile BOOL g_continuar = TRUE;

static HANDLE g_hMapFile = NULL;
static HANDLE g_hMutex = NULL;
static HANDLE g_hEvent = NULL;
static SHM_ALERTA* g_pShm = NULL;

static TCHAR g_nomeShm[128];
static TCHAR g_nomeMutex[128];
static TCHAR g_nomeEvent[128];

static int g_maxVisiveis = 5;
static int g_paginaAtual = 0;
static ALERTA_VISIVEL g_alertas[MAX_PLACARES];
static int g_totalAlertas = 0;

static HFONT g_hFontTitulo = NULL;
static HFONT g_hFontCabecalho = NULL;
static HFONT g_hFontCorpo = NULL;
static HFONT g_hFontPequeno = NULL;
static HBRUSH g_hBrushBanner = NULL;
static HBRUSH g_hBrushFundo = NULL;

static BOOL CriarRecursosGDI(void) {
    g_hFontTitulo = CreateFont(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));
    g_hFontCabecalho = CreateFont(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));
    g_hFontCorpo = CreateFont(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));
    g_hFontPequeno = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));

    g_hBrushBanner = CreateSolidBrush(COR_BANNER);
    g_hBrushFundo = CreateSolidBrush(COR_FUNDO);

    return (g_hFontTitulo != NULL && g_hFontCabecalho != NULL &&
        g_hFontCorpo != NULL && g_hFontPequeno != NULL &&
        g_hBrushBanner != NULL && g_hBrushFundo != NULL);
}

static void LibertarRecursosGDI(void) {
    if (g_hFontTitulo != NULL) { DeleteObject(g_hFontTitulo); g_hFontTitulo = NULL; }
    if (g_hFontCabecalho != NULL) { DeleteObject(g_hFontCabecalho); g_hFontCabecalho = NULL; }
    if (g_hFontCorpo != NULL) { DeleteObject(g_hFontCorpo); g_hFontCorpo = NULL; }
    if (g_hFontPequeno != NULL) { DeleteObject(g_hFontPequeno); g_hFontPequeno = NULL; }
    if (g_hBrushBanner != NULL) { DeleteObject(g_hBrushBanner); g_hBrushBanner = NULL; }
    if (g_hBrushFundo != NULL) { DeleteObject(g_hBrushFundo); g_hBrushFundo = NULL; }
}

static void PreencherRect(HDC hdc, const RECT* rc, COLORREF cor) {
    HBRUSH hb = CreateSolidBrush(cor);
    if (hb != NULL) {
        FillRect(hdc, rc, hb);
        DeleteObject(hb);
    }
}

static void DesenharTexto(HDC hdc, const RECT* rc, const TCHAR* texto, HFONT fonte,
    COLORREF cor, DWORD alinhamento) {
    HFONT antigo = (HFONT)SelectObject(hdc, fonte);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, cor);
    DrawText(hdc, texto, -1, (RECT*)rc, alinhamento | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, antigo);
}

static void DesenharTextoMultilinha(HDC hdc, const RECT* rc, const TCHAR* texto, HFONT fonte, COLORREF cor) {
    HFONT antigo = (HFONT)SelectObject(hdc, fonte);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, cor);
    DrawText(hdc, texto, -1, (RECT*)rc, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    SelectObject(hdc, antigo);
}

static void DesenharLista(HDC hdc, const RECT* prc) {
    RECT rc = *prc;
    int colId = MARGEM + 12;
    int colDur = colId + 72;
    int colMsg = colDur + 100;
    int largura = rc.right - rc.left;
    int y = rc.top;

    PreencherRect(hdc, &rc, COR_FUNDO);

    /* Banner superior */
    RECT rcBanner = { rc.left, y, rc.right, y + ALT_BANNER };
    PreencherRect(hdc, &rcBanner, COR_BANNER);
    RECT rcTitulo = { rc.left + MARGEM, y + 10, rc.right - MARGEM, y + 36 };
    DesenharTexto(hdc, &rcTitulo, _T("Monitor de Alertas Ativos"), g_hFontTitulo,
        COR_BANNER_TEXTO, DT_LEFT | DT_VCENTER);
    TCHAR subtitulo[128];
    _stprintf_s(subtitulo, 128, _T("Plataforma Placar Informativo SO2  |  %d alerta(s) em exibicao"),
        g_totalAlertas);
    RECT rcSub = { rc.left + MARGEM, y + 36, rc.right - MARGEM, y + ALT_BANNER - 6 };
    DesenharTexto(hdc, &rcSub, subtitulo, g_hFontPequeno, RGB(191, 219, 254), DT_LEFT | DT_VCENTER);
    y += ALT_BANNER + 8;

    /* Cabecalho das colunas */
    RECT rcCab = { rc.left + MARGEM, y, rc.right - MARGEM, y + ALT_CAB_COLUNAS };
    PreencherRect(hdc, &rcCab, COR_CAB_TABELA);
    {
        HPEN pen = CreatePen(PS_SOLID, 1, COR_BORDA);
        HPEN penAnt = (HPEN)SelectObject(hdc, pen);
        Rectangle(hdc, rcCab.left, rcCab.top, rcCab.right, rcCab.bottom);
        SelectObject(hdc, penAnt);
        DeleteObject(pen);
    }
    RECT rId = { colId, y + 4, colDur - 4, y + ALT_CAB_COLUNAS - 4 };
    RECT rDur = { colDur, y + 4, colMsg - 4, y + ALT_CAB_COLUNAS - 4 };
    RECT rMsg = { colMsg, y + 4, rc.right - MARGEM - 8, y + ALT_CAB_COLUNAS - 4 };
    DesenharTexto(hdc, &rId, _T("PLACAR"), g_hFontCabecalho, COR_CAB_TABELA_TX, DT_LEFT | DT_VCENTER);
    DesenharTexto(hdc, &rDur, _T("DURACAO"), g_hFontCabecalho, COR_CAB_TABELA_TX, DT_LEFT | DT_VCENTER);
    DesenharTexto(hdc, &rMsg, _T("MENSAGEM DE ALERTA"), g_hFontCabecalho, COR_CAB_TABELA_TX, DT_LEFT | DT_VCENTER);
    y += ALT_CAB_COLUNAS + 4;

    int inicio = g_paginaAtual * g_maxVisiveis;
    int fim = inicio + g_maxVisiveis;
    if (fim > g_totalAlertas) {
        fim = g_totalAlertas;
    }

    if (g_totalAlertas == 0) {
        RECT rcVazio = { rc.left + MARGEM, y + 24, rc.right - MARGEM, y + 120 };
        DesenharTextoMultilinha(hdc, &rcVazio,
            _T("Nenhum alerta ativo neste momento.\n\n")
            _T("Os alertas enviados pela Central aparecem aqui automaticamente."),
            g_hFontCorpo, COR_VAZIO);
    }
    else {
        for (int i = inicio; i < fim; i++) {
            COLORREF corLinha = (i % 2 == 0) ? COR_LINHA_PAR : COR_LINHA_IMPAR;
            RECT rcLinha = { rc.left + MARGEM, y, rc.right - MARGEM, y + ALT_LINHA };
            PreencherRect(hdc, &rcLinha, corLinha);
            {
                HPEN pen = CreatePen(PS_SOLID, 1, COR_BORDA);
                HPEN penAnt = (HPEN)SelectObject(hdc, pen);
                Rectangle(hdc, rcLinha.left, rcLinha.top, rcLinha.right, rcLinha.bottom);
                SelectObject(hdc, penAnt);
                DeleteObject(pen);
            }

            TCHAR idTxt[16];
            TCHAR durTxt[24];
            _stprintf_s(idTxt, 16, _T("#%u"), g_alertas[i].identificador);
            _stprintf_s(durTxt, 24, _T("%u s"), g_alertas[i].duracao);

            RECT rcId = { colId, y + 6, colDur - 6, y + ALT_LINHA - 6 };
            RECT rcDur = { colDur, y + 6, colMsg - 6, y + ALT_LINHA - 6 };
            RECT rcMsg = { colMsg, y + 8, rc.right - MARGEM - 10, y + ALT_LINHA - 8 };

            DesenharTexto(hdc, &rcId, idTxt, g_hFontCabecalho, COR_DESTAQUE, DT_LEFT | DT_VCENTER);
            DesenharTexto(hdc, &rcDur, durTxt, g_hFontCorpo, COR_TEXTO, DT_LEFT | DT_VCENTER);
            DesenharTextoMultilinha(hdc, &rcMsg, g_alertas[i].msg, g_hFontCorpo, COR_TEXTO);

            y += ALT_LINHA + 4;
        }
    }

    /* Rodape com paginacao */
    int totalPaginas = (g_totalAlertas > 0)
        ? ((g_totalAlertas - 1) / g_maxVisiveis + 1) : 1;
    RECT rcRodape = { rc.left, rc.bottom - ALT_RODAPE, rc.right, rc.bottom };
    PreencherRect(hdc, &rcRodape, COR_CAB_TABELA);
    TCHAR rodape[160];
    _stprintf_s(rodape, 160,
        _T("  Pagina %d de %d  |  Page Up: anterior  |  Page Down: seguinte  |  Max. visiveis: %d"),
        g_paginaAtual + 1, totalPaginas, g_maxVisiveis);
    RECT rcRodTxt = { rc.left + 8, rc.bottom - ALT_RODAPE + 8, rc.right - 8, rc.bottom - 8 };
    DesenharTexto(hdc, &rcRodTxt, rodape, g_hFontPequeno, COR_SUBTEXTO, DT_LEFT | DT_VCENTER);

    (void)largura; /* reservado para layout responsivo futuro */
}

static void FecharRecursosIPC(void) {
    if (g_pShm != NULL) {
        UnmapViewOfFile(g_pShm);
        g_pShm = NULL;
    }
    if (g_hMapFile != NULL) {
        CloseHandle(g_hMapFile);
        g_hMapFile = NULL;
    }
    if (g_hMutex != NULL) {
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
    }
    if (g_hEvent != NULL) {
        CloseHandle(g_hEvent);
        g_hEvent = NULL;
    }
}

static BOOL AbrirRecursosIPC(void) {
    FecharRecursosIPC();

    g_hMutex = OpenMutex(SYNCHRONIZE, FALSE, g_nomeMutex);
    if (g_hMutex == NULL) {
        return FALSE;
    }

    g_hEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, g_nomeEvent);
    if (g_hEvent == NULL) {
        return FALSE;
    }

    g_hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, g_nomeShm);
    if (g_hMapFile == NULL) {
        return FALSE;
    }

    g_pShm = (SHM_ALERTA*)MapViewOfFile(g_hMapFile, FILE_MAP_READ, 0, 0, sizeof(SHM_ALERTA));
    if (g_pShm == NULL) {
        return FALSE;
    }

    return TRUE;
}

static void CopiarAlertasDaSHM(void) {
    g_totalAlertas = 0;

    if (g_pShm == NULL || g_hMutex == NULL) {
        return;
    }

    if (WaitForSingleObject(g_hMutex, INFINITE) != WAIT_OBJECT_0) {
        return;
    }

    if (g_pShm->desligar) {
        ReleaseMutex(g_hMutex);
        PostMessage(g_hWnd, WM_APP_ENCERRAR, 0, 0);
        return;
    }

    for (int i = 0; i < MAX_PLACARES; i++) {
        if (g_pShm->placar[i].identificador != 0 && g_pShm->placar[i].msg[0] != _T('\0')) {
            g_alertas[g_totalAlertas].identificador = g_pShm->placar[i].identificador;
            g_alertas[g_totalAlertas].duracao = g_pShm->placar[i].duracao;
            _tcscpy_s(g_alertas[g_totalAlertas].msg, 140, g_pShm->placar[i].msg);
            g_totalAlertas++;
        }
    }

    ReleaseMutex(g_hMutex);

    int maxPaginas = (g_totalAlertas > 0)
        ? (g_totalAlertas - 1) / g_maxVisiveis
        : 0;
    if (g_paginaAtual > maxPaginas) {
        g_paginaAtual = maxPaginas;
    }
    if (g_paginaAtual < 0) {
        g_paginaAtual = 0;
    }
}

static DWORD WINAPI ThreadEsperaSHM(LPVOID lpParam) {
    (void)lpParam;

    while (g_continuar) {
        if (g_hEvent == NULL) {
            Sleep(1000);
            continue;
        }

        DWORD espera = WaitForSingleObject(g_hEvent, INFINITE);
        if (!g_continuar) {
            break;
        }

        if (espera == WAIT_OBJECT_0) {
            CopiarAlertasDaSHM();
            if (g_hWnd != NULL) {
                PostMessage(g_hWnd, WM_APP_ATUALIZAR, 0, 0);
            }
            /* Evento manual-reset: voltar a nao sinalizado para evitar ciclo ocupado */
            if (g_hEvent != NULL) {
                ResetEvent(g_hEvent);
            }
        }
    }

    return 0;
}

static void PreencherDialogoConfig(HWND hDlg) {
    TCHAR buffer[16];
    _stprintf_s(buffer, 16, _T("%d"), g_maxVisiveis);
    SetDlgItemText(hDlg, IDC_MAX_ALERTAS, buffer);
    SetDlgItemText(hDlg, IDC_NOME_SHM, g_nomeShm);
    SetDlgItemText(hDlg, IDC_NOME_MUTEX, g_nomeMutex);
    SetDlgItemText(hDlg, IDC_NOME_EVENT, g_nomeEvent);
}

static INT_PTR CALLBACK DialogoConfigProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (msg) {
    case WM_INITDIALOG:
        PreencherDialogoConfig(hDlg);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            TCHAR buffer[16];
            int maxNovo;

            GetDlgItemText(hDlg, IDC_MAX_ALERTAS, buffer, 16);
            maxNovo = _ttoi(buffer);
            if (maxNovo < 1) {
                maxNovo = 1;
            }
            if (maxNovo > MAX_PLACARES) {
                maxNovo = MAX_PLACARES;
            }

            g_maxVisiveis = maxNovo;
            GetDlgItemText(hDlg, IDC_NOME_SHM, g_nomeShm, 128);
            GetDlgItemText(hDlg, IDC_NOME_MUTEX, g_nomeMutex, 128);
            GetDlgItemText(hDlg, IDC_NOME_EVENT, g_nomeEvent, 128);

            if (!AbrirRecursosIPC()) {
                MessageBox(hDlg,
                    _T("Nao foi possivel ligar aos recursos da Central.\n")
                    _T("Confirme que a Central esta em execucao e que os nomes estao corretos."),
                    _T("Erro"), MB_OK | MB_ICONERROR);
                return TRUE;
            }

            ResetEvent(g_hEvent);
            g_paginaAtual = 0;
            CopiarAlertasDaSHM();
            InvalidateRect(g_hWnd, NULL, TRUE);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        if (!CriarRecursosGDI()) {
            MessageBox(hWnd, _T("Erro ao criar recursos graficos."), _T("Monitor"), MB_OK | MB_ICONERROR);
            return -1;
        }
        SetFocus(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_APP_ATUALIZAR:
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;

    case WM_APP_ENCERRAR:
        DestroyWindow(hWnd);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hBmpAnt = (HBITMAP)SelectObject(hdcMem, hBmp);

        DesenharLista(hdcMem, &rc);
        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hBmpAnt);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam == VK_NEXT) {
            int maxPag = (g_totalAlertas > 0)
                ? (g_totalAlertas - 1) / g_maxVisiveis
                : 0;
            if (g_paginaAtual < maxPag) {
                g_paginaAtual++;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        if (wParam == VK_PRIOR) {
            if (g_paginaAtual > 0) {
                g_paginaAtual--;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_CONFIG:
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CONFIG), hWnd, DialogoConfigProc);
            return 0;
        case IDM_ACERCA:
            MessageBox(hWnd,
                _T("Monitor de Alertas - Trabalho SO2\n\n")
                _T("Antonio Correia - 2022141330\n")
                _T("Joao Liborio - 2024128702"),
                _T("Acerca"), MB_OK | MB_ICONINFORMATION);
            return 0;
        case IDM_SAIR:
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        g_continuar = FALSE;
        if (g_hEvent != NULL) {
            SetEvent(g_hEvent);
        }
        if (g_hThreadSHM != NULL) {
            WaitForSingleObject(g_hThreadSHM, 5000);
            CloseHandle(g_hThreadSHM);
            g_hThreadSHM = NULL;
        }
        FecharRecursosIPC();
        LibertarRecursosGDI();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY _tWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPTSTR lpCmdLine, int nShow) {
    (void)hPrev;
    (void)lpCmdLine;

    g_hInst = hInst;

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
    wc.lpszClassName = CLASSE_JANELA;

    if (!RegisterClassEx(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        MessageBox(NULL, _T("Erro ao registar a classe da janela."), _T("Monitor"), MB_OK | MB_ICONERROR);
        return 1;
    }

    _tcscpy_s(g_nomeShm, 128, DEFAULT_SHM_NAME);
    _tcscpy_s(g_nomeMutex, 128, DEFAULT_MUTEX_NAME);
    _tcscpy_s(g_nomeEvent, 128, DEFAULT_EVENT_NAME);

    HWND hWnd = CreateWindowEx(
        0,
        CLASSE_JANELA,
        _T("Monitor de Alertas - Placar Informativo SO2"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 560,
        NULL, NULL, hInst, NULL);

    if (hWnd == NULL) {
        MessageBox(NULL, _T("Erro ao criar a janela principal."), _T("Monitor"), MB_OK | MB_ICONERROR);
        return 1;
    }

    g_hWnd = hWnd;
    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    if (!AbrirRecursosIPC()) {
        MessageBox(hWnd,
            _T("Central nao encontrada.\n")
            _T("Inicie central.exe e use Ficheiro > Configuracao para voltar a ligar."),
            _T("Monitor"), MB_OK | MB_ICONWARNING);
    }
    else {
        ResetEvent(g_hEvent);
        CopiarAlertasDaSHM();
        InvalidateRect(hWnd, NULL, TRUE);
    }

    g_hThreadSHM = CreateThread(NULL, 0, ThreadEsperaSHM, NULL, 0, NULL);
    if (g_hThreadSHM == NULL) {
        MessageBox(hWnd, _T("Erro ao criar thread de monitorizacao."), _T("Monitor"), MB_OK | MB_ICONERROR);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
