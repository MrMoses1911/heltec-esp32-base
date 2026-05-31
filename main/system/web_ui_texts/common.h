#pragma once

#define PULSE_WEB_UI_TEXTS_COMMON_GROUP R"html(
    common: {
      buttons: {
        login: 'Login',
        logout: 'Logout',
        view: 'Visualização',
        params: 'Configurações',
        saveAll: 'Salvar tudo',
        restart: 'Reiniciar',
        factoryReset: 'Reset de fábrica',
        exitAp: 'Encerrar AP',
        autoPair: 'Parear gateway',
        prevSlot: 'Slot anterior',
        nextSlot: 'Próximo slot',
        clearSlot: 'Limpar slot'
      },
      prompts: {
        loginUser: 'Usuário do webserver:',
        loginPass: 'Senha do webserver:'
      },
      alerts: {
        loginFailed: 'Falha no login. Verifique usuário e senha.',
        authRequired: 'Login necessário.',
        requestFailed: 'Falha na requisição.',
        saveConfirm: 'Confirma salvar todas as configurações?',
        restartConfirm: 'Confirma reiniciar o gateway?',
        factoryResetConfirm: 'Confirma reset de fábrica?',
        exitApConfirm: 'Confirma desativar o AP e encerrar o webserver?'
      },
      communicationModes: {
        0: 'LoRa desligado',
        1: 'Espelhamento de entradas',
        2: 'Comando remoto de saídas',
        3: 'Broadcast (chave de grupo)'
      }
    },
)html"
