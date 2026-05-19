#pragma once

#define PULSE_WEB_UI_TEXTS_COMMON_GROUP R"html(
    common: {
      buttons: {
        login: 'Login',
        logout: 'Logout',
        view: 'Visualizacao',
        params: 'Configuracoes',
        saveAll: 'Salvar tudo',
        restart: 'Reiniciar',
        factoryReset: 'Reset de fabrica',
        exitAp: 'Encerrar AP',
        autoPair: 'Parear gateway',
        prevSlot: 'Slot anterior',
        nextSlot: 'Proximo slot',
        clearSlot: 'Limpar slot'
      },
      prompts: {
        loginUser: 'Usuario do webserver:',
        loginPass: 'Senha do webserver:'
      },
      alerts: {
        loginFailed: 'Falha no login. Verifique usuario e senha.',
        authRequired: 'Login necessario.',
        requestFailed: 'Falha na requisicao.',
        saveConfirm: 'Confirma salvar todas as configuracoes?',
        restartConfirm: 'Confirma reiniciar o gateway?',
        factoryResetConfirm: 'Confirma reset de fabrica?',
        exitApConfirm: 'Confirma desativar o AP e encerrar o webserver?'
      },
      communicationModes: {
        0: 'LoRa desligado',
        1: 'Espelhamento de entradas',
        2: 'Comando remoto de saidas',
        3: 'Broadcast (chave de grupo)'
      }
    },
)html"
