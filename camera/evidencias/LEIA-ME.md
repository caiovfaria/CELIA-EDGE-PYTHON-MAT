# Evidência: câmera do poste com a API do João

Teste com a ESP32-CAM gravada com o `camera_poste/camera_poste.ino` (vídeo + foto para a API), ligada no Wi-Fi da equipe e com a chave da câmera colada só na placa. A chave e a senha do Wi-Fi não estão no repositório.

Trecho do Monitor Serial (copiado pelo aluno):

```
Foto 2: 13460 bytes (640x480)
API respondeu 201: {... resultado livre ... mqtt_enviado true ...}
```

- **201:** a API recebeu a foto e guardou a captura.
- **resultado livre:** o modelo de buracos não viu buraco naquela calçada.
- **mqtt_enviado true:** a API avisou o ESP32 do poste pelo MQTT.

O vídeo ao vivo também foi conferido no navegador, pelo endereço da câmera que aparece no Monitor Serial.

Antes de ir para a placa, o envio foi testado no computador contra uma API falsa, em 3 casos: chave certa (201, foto JPEG de 640x480 com os cabeçalhos `Content-Type`, `X-Chave` e `X-Poste`, e o vídeo voltando para 320x240), chave errada (401) e API fora do ar (o programa não trava). Também foi conferido que, sem chave, nada é enviado.

## Correção depois da revisão do Codex (18/11)

O Codex apontou que conferir só `foto->width == 640` não garante uma foto nova: o driver da câmera marca o quadro com o tamanho configurado na hora, então um quadro antigo do vídeo (320x240) que ainda estava na fila podia sair marcado como 640x480. Com uma câmera falsa que imita esse comportamento do driver, a versão antiga mandou mesmo um JPEG de 320x240 para a API.

A versão do repositório agora:
- usa uma trava (mutex) para o vídeo e a foto não pegarem quadros ao mesmo tempo;
- confere o retorno de `set_framesize()`;
- joga fora os quadros que já estavam na fila;
- lê o tamanho de dentro do próprio JPEG (marcador SOF) e só aceita 640x480.

Testes no computador com a versão corrigida, todos ok: os 3 casos acima e o caso sem chave; a leitura do tamanho no JPEG (640x480, 320x240, lixo e JPEG cortado); o vídeo segurando a câmera por 300 ms (a foto espera e sai em 640x480); o vídeo sem largar a câmera por 4 s (a foto desiste com aviso e nada é enviado); e a câmera recusando mudar para 640x480 (nada é enviado, nem um quadro 320x240).

O teste real com a API (o 201 acima) foi feito com a versão anterior, que é a que está gravada na placa. Antes da apresentação, grave na placa a versão do repositório.
