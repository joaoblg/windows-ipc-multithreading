================================================================================
Trabalho Laboratorial - Sistemas Operativos 2 (2025/26)
Plataforma "Placar Informativo" - Entrega Final (Metas 1, 2 e 3)
================================================================================

AUTORES:
* António Correia (2022141330)
* João Libório (2024128702)

AVALIAÇÃO E DEFESA ORAL:
Declaramos que NÃO pretendemos realizar a defesa oral individual 
presencial. Optamos por submeter o projeto para a avaliação funcional 
contínua, correspondente à cotação de 2.5 valores.

REQUISITOS IMPLEMENTADOS:

[ Meta 1 - Placar ]
* Comandos 'ligar' e 'desligar' implementados, com atribuição de identificador e encerramento limpo.
* Definição do Named Pipe via linha de comandos ou, na sua ausência, via leitura/escrita no Registry (HKCU\Software\TrabSO2 -> NPIPE).
* Utilização de duas threads dedicadas: uma para os comandos do utilizador e outra para a receção contínua.
* Controlo da duração dos alertas efetuado através de Waitable Timers, imprimindo a respetiva data/hora e a string '---' no término.

[ Meta 2 - Central e Comunicação IPC ]
* Central aceita o nome do pipe por argumento e estabelece o Named Pipe em modo bidirecional (Message).
* Central disponibiliza e processa os comandos 'alerta', 'cancelar', 'listar' e 'encerrar'.
* Processamento centralizado de pedidos: atribuição de IDs aos placares, confirmação de saída e processamento de expiração natural dos alertas.
* Estrutura de dados centralizada e protegida com Critical Sections para garantir a coerência do estado atual.
* Placar adaptado para enviar/receber respostas via Named Pipes, incluindo o "eco" de confirmação ao receber novos alertas, cancelamentos e ordens de encerramento.

[ Meta 3 - Monitor e Memória Partilhada ]
* Interface gráfica Win32 desenvolvida, contendo o menu Ficheiro com as opções Configuração (DialogBox), Acerca (MessageBox) e Sair.
* Janela principal exibe os alertas ativos com respetivo ID, Duração e Mensagem.
* Paginação totalmente funcional caso os alertas excedam o limite da vista, controlada pelas teclas Page Down e Page Up.
* Partilha de dados Central -> Monitor efetuada estritamente por Memória Partilhada (SHM), utilizando arrays estáticos e garantindo a ausência de ponteiros.
* Sincronização entre processos feita usando Mutex e Evento. A thread do Monitor bloqueia passivamente à espera do Evento, eliminando a utilização ineficiente de "Polling".