#include "../Comum/header.h" 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

HANDLE hPipe = INVALID_HANDLE_VALUE;
BOOL sair = FALSE;
DWORD meuIdentificador = 0;

HANDLE hTimer;
HANDLE hNovoAlertaEvent;
DWORD alertaDuracao = 0;

void ImprimirHora() {
    SYSTEMTIME st; GetLocalTime(&st);
    _tprintf(_T("\n%02d/%02d/%04d (%02d:%02d:%02d): "),
        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
}

DWORD WINAPI ThreadTemporizador(LPVOID lpParam) {
    LARGE_INTEGER dueTime;
    while (!sair) {
        HANDLE handles[2] = { hNovoAlertaEvent, hTimer };
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (sair) break;

        if (waitResult == WAIT_OBJECT_0) {
            dueTime.QuadPart = -(LONGLONG)alertaDuracao * 10000000LL;
            SetWaitableTimer(hTimer, &dueTime, 0, NULL, NULL, 0);
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            ImprimirHora(); _tprintf(_T("'---'\n> "));
            MSG_CMD msgFim; msgFim.tipo = M_FIM_ALERTA;
            DWORD escritos;
            WriteFile(hPipe, &msgFim, sizeof(MSG_CMD), &escritos, NULL);
        }
    }
    return 0;
}

DWORD WINAPI ThreadRecebeAlertas(LPVOID lpParam) {
    BYTE buffer[sizeof(MSG_ALERTA)];
    DWORD lidos, escritos;

    while (!sair) {
        DWORD bytesDisponiveis = 0;

        if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesDisponiveis, NULL)) {
            _tprintf(_T("\n[Placar] Ligacao perdida com a Central. A terminar...\n"));
            exit(1);
        }

        if (bytesDisponiveis > 0) {
            if (ReadFile(hPipe, buffer, sizeof(buffer), &lidos, NULL) && lidos > 0) {
                BYTE tipo = buffer[0];

                if (tipo == M_ATRIBUIR_ID) {
                    MSG_ID* mId = (MSG_ID*)buffer;
                    meuIdentificador = mId->identificador;
                    _tprintf(_T("\n[Placar] Registado com sucesso! O meu ID e: %d\n> "), meuIdentificador);
                }
                else if (tipo == M_NOVO_ALERTA) {
                    WriteFile(hPipe, buffer, lidos, &escritos, NULL); // Eco de confirma��o
                    MSG_ALERTA* al = (MSG_ALERTA*)buffer;
                    ImprimirHora(); _tprintf(_T("'%s'\n> "), al->msg);
                    alertaDuracao = al->duracao;
                    SetEvent(hNovoAlertaEvent);
                }
                else if (tipo == M_CANCELAR_ALERTA) {
                    WriteFile(hPipe, buffer, lidos, &escritos, NULL); // Eco de confirma��o
                    CancelWaitableTimer(hTimer);
                    ImprimirHora(); _tprintf(_T("'---' (Alerta Cancelado)\n> "));
                }
                else if (tipo == M_DESLIGAR) {
                    _tprintf(_T("\n[Placar] Confirmacao de desligar recebida. A sair...\n"));
                    sair = TRUE;
                    break;
                }
                else if (tipo == M_ENCERRAR) {
                    _tprintf(_T("\n[Placar] Central encerrou o sistema. A terminar...\n"));
                    exit(0);
                }
            }
        }
        else {
            Sleep(50);
        }
    }
    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
    TCHAR nomePipe[256] = _T("");
    TCHAR pipePath[300];
    TCHAR comando[50];
    DWORD escritos;
    HKEY hKey;

    hNovoAlertaEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

    if (argc > 1) {
        _tcscpy_s(nomePipe, 256, argv[1]);
        if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\TrabSO2"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueEx(hKey, _T("NPIPE"), 0, REG_SZ, (const BYTE*)nomePipe, (DWORD)(_tcslen(nomePipe) + 1) * sizeof(TCHAR));
            RegCloseKey(hKey);
        }
    }
    else {
        DWORD size = sizeof(nomePipe);
        if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\TrabSO2"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueEx(hKey, _T("NPIPE"), NULL, NULL, (LPBYTE)nomePipe, &size);
            RegCloseKey(hKey);
        }
    }

    if (_tcslen(nomePipe) == 0) _tcscpy_s(nomePipe, 256, DEFAULT_PIPE_NAME);

    _stprintf_s(pipePath, 300, _T("\\\\.\\pipe\\%s"), nomePipe);
    _tprintf(_T("NamedPipe = '%s'\n"), nomePipe);

    if (!WaitNamedPipe(pipePath, NMPWAIT_WAIT_FOREVER)) {
        _tprintf(_T("Erro: Central nao encontrada.\n")); return 1;
    }

    hPipe = CreateFile(pipePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) return 1;

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

    CreateThread(NULL, 0, ThreadRecebeAlertas, NULL, 0, NULL);
    CreateThread(NULL, 0, ThreadTemporizador, NULL, 0, NULL);

    _tprintf(_T("Placar ativo. Comandos: 'ligar', 'desligar'\n> "));

    while (!sair) {
        if (_tscanf_s(_T("%s"), comando, (unsigned)_countof(comando)) <= 0) continue;

        if (_tcscmp(comando, _T("ligar")) == 0) {
            if (meuIdentificador == 0) {
                MSG_CMD msg; msg.tipo = M_LIGAR;
                WriteFile(hPipe, &msg, sizeof(MSG_CMD), &escritos, NULL);
            }
            else {
                _tprintf(_T("Ja esta ligado com o ID: %d\n> "), meuIdentificador);
            }
        }
        else if (_tcscmp(comando, _T("desligar")) == 0) {
            MSG_CMD msg; msg.tipo = M_DESLIGAR;
            WriteFile(hPipe, &msg, sizeof(MSG_CMD), &escritos, NULL);
            _tprintf(_T("A aguardar confirmacao da Central...\n"));
        }
    }

    CloseHandle(hPipe);
    CloseHandle(hNovoAlertaEvent);
    CloseHandle(hTimer);
    return 0;
}