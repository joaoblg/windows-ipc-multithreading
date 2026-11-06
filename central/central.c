#include "../Comum/header.h"
#include <stdio.h>
#include <process.h>

typedef struct {
    DWORD id;
    HANDLE hPipe;
    BOOL ativo;
    TCHAR alertaAtual[140];
    DWORD duracaoAlerta;
    BOOL temAlerta;
} CLIENTE_PLACAR;

static CLIENTE_PLACAR listaPlacares[MAX_PLACARES];
static DWORD contadorId = 1;
static CRITICAL_SECTION csPlacares;
static BOOL plataformaAtiva = TRUE;

/* Recursos IPC com o monitor */
static HANDLE hMapFile = NULL;
static HANDLE hMutexSHM = NULL;
static HANDLE hEventSHM = NULL;
static SHM_ALERTA* pShm = NULL;

static void AtualizarMemoriaPartilhada(void) {
    if (pShm == NULL || hMutexSHM == NULL || hEventSHM == NULL) {
        return;
    }

    if (WaitForSingleObject(hMutexSHM, INFINITE) != WAIT_OBJECT_0) {
        return;
    }

    if (ResetEvent(hEventSHM) == 0) {
        ReleaseMutex(hMutexSHM);
        return;
    }

    for (int j = 0; j < MAX_PLACARES; j++) {
        pShm->placar[j].identificador = 0;
        pShm->placar[j].duracao = 0;
        pShm->placar[j].msg[0] = _T('\0');
    }

    int idx = 0;
    EnterCriticalSection(&csPlacares);
    for (int i = 0; i < MAX_PLACARES && idx < MAX_PLACARES; i++) {
        if (listaPlacares[i].ativo && listaPlacares[i].temAlerta) {
            pShm->placar[idx].identificador = listaPlacares[i].id;
            pShm->placar[idx].duracao = listaPlacares[i].duracaoAlerta;
            _tcscpy_s(pShm->placar[idx].msg, 140, listaPlacares[i].alertaAtual);
            idx++;
        }
    }
    LeaveCriticalSection(&csPlacares);

    ReleaseMutex(hMutexSHM);
    SetEvent(hEventSHM);
}

static BOOL InicializarMemoriaPartilhada(void) {
    hMutexSHM = CreateMutex(NULL, FALSE, DEFAULT_MUTEX_NAME);
    if (hMutexSHM == NULL) {
        _tprintf(_T("Erro ao criar mutex do monitor.\n"));
        return FALSE;
    }

    hEventSHM = CreateEvent(NULL, TRUE, FALSE, DEFAULT_EVENT_NAME);
    if (hEventSHM == NULL) {
        _tprintf(_T("Erro ao criar evento do monitor.\n"));
        return FALSE;
    }

    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
        sizeof(SHM_ALERTA), DEFAULT_SHM_NAME);
    if (hMapFile == NULL) {
        _tprintf(_T("Erro ao criar memoria partilhada do monitor.\n"));
        return FALSE;
    }

    pShm = (SHM_ALERTA*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHM_ALERTA));
    if (pShm == NULL) {
        _tprintf(_T("Erro ao mapear memoria partilhada do monitor.\n"));
        return FALSE;
    }

    if (WaitForSingleObject(hMutexSHM, INFINITE) == WAIT_OBJECT_0) {
        ZeroMemory(pShm, sizeof(SHM_ALERTA));
        pShm->desligar = FALSE;
        ReleaseMutex(hMutexSHM);
    }

    ResetEvent(hEventSHM);
    return TRUE;
}

static void LibertarMemoriaPartilhada(void) {
    if (pShm != NULL) {
        UnmapViewOfFile(pShm);
        pShm = NULL;
    }
    if (hMapFile != NULL) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
    if (hMutexSHM != NULL) {
        CloseHandle(hMutexSHM);
        hMutexSHM = NULL;
    }
    if (hEventSHM != NULL) {
        CloseHandle(hEventSHM);
        hEventSHM = NULL;
    }
}

static void NotificarEncerramentoMonitor(void) {
    if (pShm == NULL || hMutexSHM == NULL || hEventSHM == NULL) {
        return;
    }

    if (WaitForSingleObject(hMutexSHM, INFINITE) != WAIT_OBJECT_0) {
        return;
    }

    ResetEvent(hEventSHM);
    pShm->desligar = TRUE;
    ReleaseMutex(hMutexSHM);
    SetEvent(hEventSHM);
}

unsigned __stdcall ThreadTrataPlacar(void* data) {
    HANDLE hPipe = (HANDLE)data;
    BYTE buffer[sizeof(MSG_ALERTA)];
    DWORD lidos, escritos, meuId = 0;
    int indexLista = -1;

    while (plataformaAtiva) {
        DWORD bytesDisponiveis = 0;

        if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesDisponiveis, NULL)) {
            break;
        }

        if (bytesDisponiveis > 0) {
            if (ReadFile(hPipe, buffer, sizeof(buffer), &lidos, NULL) && lidos > 0) {
                BYTE tipo = buffer[0];

                if (tipo == M_LIGAR) {
                    EnterCriticalSection(&csPlacares);
                    meuId = contadorId++;
                    for (int i = 0; i < MAX_PLACARES; i++) {
                        if (!listaPlacares[i].ativo) {
                            listaPlacares[i].id = meuId;
                            listaPlacares[i].hPipe = hPipe;
                            listaPlacares[i].ativo = TRUE;
                            listaPlacares[i].temAlerta = FALSE;
                            listaPlacares[i].duracaoAlerta = 0;
                            _tcscpy_s(listaPlacares[i].alertaAtual, 140, _T("---"));
                            indexLista = i;
                            break;
                        }
                    }
                    LeaveCriticalSection(&csPlacares);

                    MSG_ID resposta;
                    resposta.tipo = M_ATRIBUIR_ID;
                    resposta.identificador = meuId;
                    WriteFile(hPipe, &resposta, sizeof(MSG_ID), &escritos, NULL);
                    _tprintf(_T("\n[Central] Placar registado com ID: %u\n> "), meuId);
                }
                else if (tipo == M_DESLIGAR) {
                    WriteFile(hPipe, buffer, lidos, &escritos, NULL);
                    _tprintf(_T("\n[Central] Placar %u solicitou desligar.\n> "), meuId);
                    break;
                }
                else if (tipo == M_FIM_ALERTA) {
                    EnterCriticalSection(&csPlacares);
                    if (indexLista != -1) {
                        listaPlacares[indexLista].temAlerta = FALSE;
                        listaPlacares[indexLista].duracaoAlerta = 0;
                        _tcscpy_s(listaPlacares[indexLista].alertaAtual, 140, _T("---"));
                    }
                    LeaveCriticalSection(&csPlacares);
                    AtualizarMemoriaPartilhada();
                    _tprintf(_T("\n[Central] Alerta expirou no placar %u.\n> "), meuId);
                }
                else if (tipo == M_NOVO_ALERTA) {
                    MSG_ALERTA* al = (MSG_ALERTA*)buffer;
                    EnterCriticalSection(&csPlacares);
                    if (indexLista != -1) {
                        listaPlacares[indexLista].temAlerta = TRUE;
                        listaPlacares[indexLista].duracaoAlerta = al->duracao;
                        _tcscpy_s(listaPlacares[indexLista].alertaAtual, 140, al->msg);
                    }
                    LeaveCriticalSection(&csPlacares);
                    AtualizarMemoriaPartilhada();
                    _tprintf(_T("\n[Central] Placar %u confirmou rececao do alerta.\n> "), meuId);
                }
                else if (tipo == M_CANCELAR_ALERTA) {
                    EnterCriticalSection(&csPlacares);
                    if (indexLista != -1) {
                        listaPlacares[indexLista].temAlerta = FALSE;
                        listaPlacares[indexLista].duracaoAlerta = 0;
                        _tcscpy_s(listaPlacares[indexLista].alertaAtual, 140, _T("---"));
                    }
                    LeaveCriticalSection(&csPlacares);
                    AtualizarMemoriaPartilhada();
                    _tprintf(_T("\n[Central] Placar %u confirmou cancelamento.\n> "), meuId);
                }
            }
        }
        else {
            Sleep(50);
        }
    }

    EnterCriticalSection(&csPlacares);
    if (indexLista != -1) {
        listaPlacares[indexLista].ativo = FALSE;
    }
    LeaveCriticalSection(&csPlacares);

    AtualizarMemoriaPartilhada();

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    return 0;
}

unsigned __stdcall ThreadServidor(void* data) {
    TCHAR* nomePipe = (TCHAR*)data;
    TCHAR pipeFull[300];
    _stprintf_s(pipeFull, 300, _T("\\\\.\\pipe\\%s"), nomePipe);

    while (plataformaAtiva) {
        HANDLE hPipe = CreateNamedPipe(pipeFull, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, sizeof(MSG_ALERTA), sizeof(MSG_ALERTA), 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) {
            break;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected) {
            _beginthreadex(NULL, 0, ThreadTrataPlacar, (void*)hPipe, 0, NULL);
        }
        else {
            CloseHandle(hPipe);
        }
    }
    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
    TCHAR cmd[100];
    TCHAR nomePipe[256];

    if (argc >= 2) {
        _tcscpy_s(nomePipe, 256, argv[1]);
    }
    else {
        _tcscpy_s(nomePipe, 256, DEFAULT_PIPE_NAME);
    }

    InitializeCriticalSection(&csPlacares);
    for (int i = 0; i < MAX_PLACARES; i++) {
        listaPlacares[i].ativo = FALSE;
    }

    if (!InicializarMemoriaPartilhada()) {
        DeleteCriticalSection(&csPlacares);
        return 1;
    }

    AtualizarMemoriaPartilhada();

    _tprintf(_T("Central de Comando SO2 - Pipe: \\\\.\\pipe\\%s\n"), nomePipe);
    _tprintf(_T("SHM monitor: %s\n"), DEFAULT_SHM_NAME);
    _beginthreadex(NULL, 0, ThreadServidor, (void*)nomePipe, 0, NULL);

    while (plataformaAtiva) {
        _tprintf(_T("> "));
        if (_tscanf_s(_T("%s"), cmd, (unsigned)_countof(cmd)) <= 0) {
            continue;
        }

        if (_tcscmp(cmd, _T("listar")) == 0) {
            EnterCriticalSection(&csPlacares);
            _tprintf(_T("\n--- Lista de Placares Ativos ---\n"));
            for (int i = 0; i < MAX_PLACARES; i++) {
                if (listaPlacares[i].ativo) {
                    _tprintf(_T("ID: %u | Alerta: %s\n"), listaPlacares[i].id, listaPlacares[i].alertaAtual);
                }
            }
            LeaveCriticalSection(&csPlacares);
        }
        else if (_tcscmp(cmd, _T("alerta")) == 0) {
            TCHAR msg[140];
            DWORD duracao, idDest;
            if (_tscanf_s(_T(" \"%[^\"]\" %u %u"), msg, (unsigned)_countof(msg), &duracao, &idDest) == 3) {
                MSG_ALERTA na;
                na.tipo = M_NOVO_ALERTA;
                na.duracao = duracao;
                _tcscpy_s(na.msg, 140, msg);

                EnterCriticalSection(&csPlacares);
                for (int i = 0; i < MAX_PLACARES; i++) {
                    if (listaPlacares[i].ativo && (idDest == 0 || listaPlacares[i].id == idDest)) {
                        DWORD esc;
                        WriteFile(listaPlacares[i].hPipe, &na, sizeof(MSG_ALERTA), &esc, NULL);
                        _tprintf(_T("A enviar alerta para o Placar %u...\n"), listaPlacares[i].id);
                    }
                }
                LeaveCriticalSection(&csPlacares);
            }
            else {
                while (getchar() != '\n');
                _tprintf(_T("Erro de sintaxe. Uso: alerta \"<msg>\" <duracao> <id>\n"));
            }
        }
        else if (_tcscmp(cmd, _T("cancelar")) == 0) {
            DWORD idDest;
            if (_tscanf_s(_T("%u"), &idDest) == 1) {
                MSG_CMD cc;
                cc.tipo = M_CANCELAR_ALERTA;
                EnterCriticalSection(&csPlacares);
                for (int i = 0; i < MAX_PLACARES; i++) {
                    if (listaPlacares[i].ativo && listaPlacares[i].id == idDest) {
                        DWORD esc;
                        WriteFile(listaPlacares[i].hPipe, &cc, sizeof(MSG_CMD), &esc, NULL);
                    }
                }
                LeaveCriticalSection(&csPlacares);
            }
        }
        else if (_tcscmp(cmd, _T("encerrar")) == 0) {
            MSG_CMD enc;
            enc.tipo = M_ENCERRAR;
            EnterCriticalSection(&csPlacares);
            for (int i = 0; i < MAX_PLACARES; i++) {
                if (listaPlacares[i].ativo) {
                    DWORD esc;
                    WriteFile(listaPlacares[i].hPipe, &enc, sizeof(MSG_CMD), &esc, NULL);
                }
            }
            LeaveCriticalSection(&csPlacares);

            NotificarEncerramentoMonitor();
            plataformaAtiva = FALSE;
            _tprintf(_T("A encerrar a plataforma...\n"));
            break;
        }
    }

    LibertarMemoriaPartilhada();
    DeleteCriticalSection(&csPlacares);
    return 0;
}
