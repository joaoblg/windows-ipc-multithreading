#ifndef HEADER_H
#define HEADER_H

#include <windows.h>
#include <tchar.h>

#pragma once

#define MAX_PLACARES 20
#define DEFAULT_PIPE_NAME _T("tubo_so2")

/* Nomes predefinidos dos objectos IPC Central <-> Monitor */
#define DEFAULT_SHM_NAME   _T("Local\\TrabSO2_SHM_Alertas")
#define DEFAULT_MUTEX_NAME _T("Local\\TrabSO2_Mutex_SHM")
#define DEFAULT_EVENT_NAME _T("Local\\TrabSO2_Event_SHM")

/* Tipos de mensagens (named pipes) */
#define M_LIGAR           1
#define M_DESLIGAR        2
#define M_FIM_ALERTA      3
#define M_NOVO_ALERTA     4
#define M_CANCELAR_ALERTA 5
#define M_ENCERRAR        6
#define M_ATRIBUIR_ID     7

typedef struct {
    BYTE tipo;
} MSG_CMD;

typedef struct {
    BYTE tipo;
    DWORD identificador;
} MSG_ID;

typedef struct {
    BYTE tipo;
    TCHAR msg[140];
    DWORD duracao;
} MSG_ALERTA;

/* Memoria partilhada Central -> Monitor */
typedef struct {
    struct {
        DWORD identificador;
        DWORD duracao;
        TCHAR msg[140];
    } placar[MAX_PLACARES];
    BOOL desligar;
} SHM_ALERTA;

#endif
