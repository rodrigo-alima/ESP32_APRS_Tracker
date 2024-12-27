# ESP32 APRS Tracker

Este projeto implementa um rastreador APRS (Automatic Packet Reporting System) utilizando um ESP32. Ele combina conectividade GPS e uma interface web para configuração, além de permitir a transmissão de pacotes APRS.

## Recursos

- **Acesso via Web**: Configure parâmetros do APRS, como indicativo, símbolo, SSID e mensagem, através de uma página web.
- **GPS**: Obtenha localização em tempo real usando um módulo GPS.
- **APRS**: Transmissão de localização e mensagens via protocolo APRS.
- **Memória Persistente**: Salva configurações em memória não volátil utilizando a biblioteca Preferences.
- **Servidor Web**: Funciona como um ponto de acesso (Access Point) para facilitar a configuração inicial.

## Hardware Necessário

- ESP32
- Módulo GPS compatível
- Antena para transmissão de APRS
- Fonte de alimentação estável para o ESP32

## Configuração do Projeto

1. **Bibliotecas Necessárias**:
   - [Arduino](https://www.arduino.cc/)
   - TinyGPS++
   - WiFi (incluído no framework do ESP32)
   - Preferences
   - WebServer

2. **Pinos Utilizados**:
   - **GPS RX**: GPIO 16
   - **GPS TX**: GPIO 17
   - **Depuração RX**: GPIO 3
   - **Depuração TX**: GPIO 1
   - **PTT**: GPIO 33
   - **Saída de aúdio**: GPIO 26
   - Aguarde que colocarei o esquema do filtro de áudio e do PTT. 

3. **Access Point Padrão**:
   - SSID: `APRS`
   - Senha: `12345678`

## Funcionalidades

- **Servidor Web**:
  - Configuração do indicativo, símbolo, SSID e mensagem.
  - Controle do intervalo de transmissão.
  - Página de status com dados atuais.
  
- **GPS**:
  - Leitura e formatação de coordenadas no padrão APRS.
  - Validação de dados GPS.

- **APRS**:
  - Transmissão de pacotes com localização e mensagem personalizada.

## Como Usar

1. **Inicialização**:
   - Conecte-se ao Access Point criado pelo ESP32 (`APRS`).
   - Acesse `http://192.168.4.1` no navegador.

2. **Configuração**:
   - Altere os valores de acordo com a sua necessidade.
   - Salve as configurações clicando em "Salvar".

3. **Transmissão**:
   - O ESP32 transmitirá automaticamente os dados configurados no intervalo especificado.

## Licença

Este projeto é distribuído sob a licença GPL. Consulte o arquivo `LICENSE` para mais detalhes.

---

### Contato

Criado por **PY4RDG**  
Email: py4rdg@gmail.com
