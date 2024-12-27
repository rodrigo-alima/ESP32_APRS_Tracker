#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <Arduino.h>
#include <LibAPRS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <Preferences.h>
#include <WebServer.h>

//Configurações do Access Point
const char *ap_rede = "APRS";
const char *ap_password = "12345678";

// Configurações de transmissão
int intervalo_de_tx = 10000;  // 10 segundos em milissegundos

//Variáveis de temporização do servidor Web
unsigned long lastConeectionTime = 0;
const unsigned long  timeoutPeriod = 120000;

// Configuração das portas seriais do ESP32
#define GPS_RX 16  // Pino RX do GPS
#define GPS_TX 17  // Pino TX do GPS
#define DEBUG_RX 3 // Pino RX para depuração (Serial0)
#define DEBUG_TX 1 // Pino TX para depuração (Serial0)

// Configurações do radioamador
char indicativo[7];
int ssid = 7;
char simbolo = '<';
char mensagem[51];
char latGPS[9];
char longGPS[10];

//Criação do objeto preferences
Preferences preferences;

//Criação do servidor na porta 80
WebServer server(80);

// Instâncias de HardwareSerial
HardwareSerial gpsSerial(1);  // Usar UART1

// Objeto TinyGPS++
TinyGPSPlus gps;

//Assinaturas de funções
extern "C" void aprs_msg_callback(struct AX25Msg *msg);
void converterLatitude(double latitude, char *latitudeFormatada);
void converterLongitude(double longitude, char *longitudeFormatada);
void abrirDadosSalvos();
void handleInicio();
void handleSalvar();
void handleSalvo();
void handleDesligar();
void salvarDados();

//Função que rodará no core0 e ficará por conta do servidor Web para configuração
void TaskCore0(void *pvParameters){
    unsigned long tempoAtual = 0;
    
    //UBaseType_t watermark;
    while(1) {
      server.handleClient();
      // As linhas abaixo servem para calcular o tamanho do stack necessário para a função 
      //watermark = uxTaskGetStackHighWaterMark(NULL);
      //Serial.printf("Stack watermark: %d bytes\n", watermark * 4); // multiplica por 4 pois cada word tem 4 bytes no ESP32
      vTaskDelay(pdMS_TO_TICKS(20));
      tempoAtual = millis();
      if((tempoAtual - lastConeectionTime) > timeoutPeriod){
        Serial.println("Nenhuma conexão por 2 minutos. Desligando o servidor...");
        handleDesligar();
      }
    }
}

void setup() {
  
  delay(5000);

  // Inicializar serial de depuração
  Serial.begin(115200, SERIAL_8N1, DEBUG_RX, DEBUG_TX);

  // Inicializa o sistema de preferências
  //Caso não exista ele cria com dados padrão
  if(!preferences.begin("config", false)){
      Serial.println("Falha ao criar o espaço de preferências");
      return;
  }

  // Checa se é o primeiro boot e caso seja ele grava dados padrão na memória
  if(!preferences.getBool("inicializado", false)){
      Serial.println("Primeira inicialização - Preenchendo dados padrão");
      preferences.putString("indicativo", "PY4RDG");
      preferences.putInt("ssid", 7);
      preferences.putChar("simbolo",'<');
      preferences.putString("mensagem", "Teste de multitarefa sem html");
      preferences.putInt("intervalo_tx", 15000);
      preferences.putBool("inicializado", true);
  }

  //Abre os dados salvos
  abrirDadosSalvos();

  //Inicia o modo Access point
  WiFi.softAP(ap_rede, ap_password);
  Serial.println("Access Point Iniciado");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // Configura as rotas do servidor
  server.on("/", HTTP_GET, handleInicio);
  server.on("/salvar", HTTP_POST, handleSalvar);
  server.on("/salvo", HTTP_GET, handleSalvo);
  server.on("/desligar", HTTP_GET, handleDesligar);

  //Inicia o servidor
  server.begin();

  //Cria o access point no core 0
  xTaskCreatePinnedToCore(TaskCore0,"Access Point", 8192, NULL, 0, NULL, 0);

  // Inicializar serial do GPS
  Serial.println("Inicializando módulo GPS...");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  //Inicia o aprs
  APRS_init(REF_3V3, true);

  // Mensagem inicial
  Serial.println("Transmissor APRS inicializado!");

}

void loop() {

  bool transmitido = false;

  // Ler dados do GPS
  while(gpsSerial.available() > 0 && transmitido == false) {
    if (gps.encode(gpsSerial.read())) {
      Serial.print("Número de satélites visíveis: ");
      Serial.println(gps.satellites.value());
      if (gps.location.isValid()) {

        //Converte do formato NMEA para o formato do APRS
        converterLatitude(gps.location.lat(), latGPS);
        converterLongitude(gps.location.lng(), longGPS);

        //Configura os parâmetros de transmissão
        APRS_setCallsign(indicativo, ssid);
        APRS_setDestination((char*) "APRS", 0);
        APRS_useAlternateSymbolTable(false);
        APRS_setSymbol(simbolo);
        APRS_setLat(latGPS);
        APRS_setLon(longGPS);
        APRS_setPath1((char*) "WIDE1", 1);
        APRS_setPath2((char*) "WIDE2", 1);

        //Envia a localização
        APRS_sendLoc((void*) mensagem, sizeof(mensagem));
        
        Serial.println("Dados enviados via APRS");
        Serial.print("Indicativo: ");
        Serial.println(indicativo);
        Serial.print("-");
        Serial.println(ssid);
        Serial.print("Latitude: ");
        Serial.println(latGPS);
        Serial.print("Longitude: ");
        Serial.println(longGPS);
        Serial.print("Mensagem: ");
        Serial.println(mensagem);
        transmitido = true;
        
      } else {
        Serial.println("Dados de GPS inválidos!");
      }
    }

  }

  transmitido = false;
 
  /*Somente para testes
  APRS_setCallsign(indicativo, ssid);
  APRS_setDestination((char*) "APRS", 0);
  APRS_useAlternateSymbolTable(false); //Usa os símbolos /'?'
  APRS_setSymbol(simbolo);
  APRS_setLat((char*) "1951.05S");
  APRS_setLon((char*) "04402.17W");
  APRS_setPath1((char*) "WIDE1", 1);
  APRS_setPath2((char*) "WIDE2", 1);
  APRS_sendLoc((void*) mensagem, sizeof(mensagem));
  APRS_printSettings();
  Serial.println(mensagem);
  */

  delay(intervalo_de_tx);
  Serial.println("Fim de Loop");
}

void converterLatitude(double latitude, char *latitudeFormatada){
    
    //Converte a latitude
    double latitudeAbsoluta = abs(latitude);
    int latitudeGraus = (int) latitudeAbsoluta;
    double latitudeMinutos = (latitudeAbsoluta - latitudeGraus) * 60;


    //Coloca no formato APRS
    snprintf(latitudeFormatada, 
      9,
      "%02d%05.2f%c",
      latitudeGraus,
      latitudeMinutos,
      (latitude >= 0) ? 'N' : 'S'
    );
}

void converterLongitude(double longitude, char *longitudeFormatada){
  
  //Converte a longitude
  double longitudeAbsoluta = abs(longitude);
  int longitudeGraus = (int) longitudeAbsoluta;
  double longitudeMinutos = (longitudeAbsoluta - longitudeGraus) * 60;
  
  //Coloca no formato APRS
  snprintf(longitudeFormatada,
    10,
    "%03d%05.2f%c",
    longitudeGraus,
    longitudeMinutos,
    (longitude >= 0) ? 'E' : 'W');
}

// Função de callback para processar mensagens APRS
void aprs_msg_callback(struct AX25Msg *msg) {
  /*
  Serial.println("Pacote APRS Recebido:");
  
  // Detalhes do remetente
  Serial.print("De: ");
  Serial.print(msg->src.call);
  Serial.print("-");
  Serial.println(msg->src.ssid);
  
  // Detalhes do destinatário
  Serial.print("Para: ");
  Serial.print(msg->dst.call);
  Serial.print("-");
  Serial.println(msg->dst.ssid);
  
  // Conteúdo da mensagem
  Serial.println("Mensagem:");
  for (int i = 0; i < msg->len; i++) {
    Serial.print((char)msg->info[i]);
  }
  Serial.println("\n---"); */
}

void handleInicio(){
  String html = "<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>Configuração ESP32</title>"
                  "<style>"
                  "body footer { font-family: Arial, sans-serif; margin: 20px; }"
                  ".form-group { margin-bottom: 15px; }"
                  "label { display: block; margin-bottom: 5px; }"
                  "input[type='text'] { width: 100%; padding: 8px; }"
                  "button { background-color: #4CAF50; color: white; padding: 10px 15px; border: none; cursor: pointer; }"
                  ".status { margin-top: 10px; padding: 10px; background-color: #f0f0f0; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<h2>Configuração ESP32 Tracker</h2>"
                  "<form action='/salvar' method='POST'>"
                  "<div class='form-group'>"
                  "<label for='indicativo'>Indicativo:</label>"
                  "<input type='text' id='indicativo' name='indicativo' maxlength='" + String(7) + "' value='" + indicativo + "'>"
                  "</div>"
                  "<div class='form-group'>"
                  "<label for='ssid'>SSID:</label>"
                  "<input type='text' id='ssid' name='ssid' maxlength='" + String(1) + "' value='" + String(ssid) + "'>"
                  "</div>"
                  "<div class='form-group'>"
                  "<label for='mensagem'>Símbolo:</label>"
                  "<input type='text' id='simbolo' name='simbolo' maxlength='" + String(1) + "' value='" + simbolo + "'>"
                  "</div>"
                  "<div class='form-group'>"
                  "<label for='mensagem'>Intervalo de TX (segundos):</label>"
                  "<input type='text' id='intervalo' name='intervalo' maxlength='" + String(3) + "' value='" + String(intervalo_de_tx / 1000) + "'>"
                  "</div>"
                  "<div class='form-group'>"
                  "<label for='mensagem'>Mensagem:</label>"
                  "<input type='text' id='mensagem' name='mensagem' maxlength='" + String(50) + "' value='" + mensagem + "'>"
                  "</div>"
                  "<button type='submit'>Salvar</button>"
                  "</form>"
                  "<h2><a href='/desligar'>Desligar Servidor Web</a><h2>"
                  "<div class='status'>"
                  "<strong>Valores atuais:</strong><br>"
                  "Indicativo: " + indicativo + "<br>"
                  "Símbolo: " + simbolo + "<br>"
                  "SSID: " + ssid + "<br>"
                  "Intervalo de transmissão: " + (intervalo_de_tx / 1000) + " segundos<br>" 
                  "Mensagem: " + mensagem + "<br>"
                  "</div>"
                  "<footer>"
                  "<h3>Criado por PY4RDG</h3>"
                  "<h3>py4rdg@gmail.com</h3>"
                  "</footer>"
                  "</body>"
                  "</html>";

  //Coloca o valor do tempo atual na variável última conexão
  lastConeectionTime = millis();

  server.send(200, "text/html", html);
}

void handleSalvar(){
    bool dadosAlterados = false;
  
  if (server.hasArg("indicativo")) {
    String novoIndicativo = server.arg("indicativo");
    if (novoIndicativo != indicativo) {
      strncpy(indicativo, novoIndicativo.c_str(), sizeof(indicativo)-1);
      indicativo[sizeof(indicativo)-1] = '\0';
      dadosAlterados = true;
    }
  }
  
  if (server.hasArg("ssid")) {
    String novoSSID = server.arg("ssid");
    if (novoSSID != String(ssid)) {
      ssid = atoi(novoSSID.c_str());
      dadosAlterados = true;
    }
  }

  if (server.hasArg("simbolo")){
    String novoSimbolo= server.arg("simbolo");
    if (novoSimbolo != String(simbolo)){
      simbolo = (char) novoSimbolo[0];
      dadosAlterados = true;
    }
  }

  if (server.hasArg("intervalo")) {
    String novoIntervalo = server.arg("intervalo");
    if (novoIntervalo != String(intervalo_de_tx)) {
      intervalo_de_tx = atoi(novoIntervalo.c_str()) * 1000 ;
      dadosAlterados = true;
    }
  }
  
  if (server.hasArg("mensagem")) {
    String novaMensagem = server.arg("mensagem");
    if (novaMensagem != mensagem) {
      strncpy(mensagem, novaMensagem.c_str(), sizeof(mensagem)-1);
      mensagem[sizeof(mensagem)-1]='\0';
      dadosAlterados = true;
    }
  }
  
  if (dadosAlterados) {
    salvarDados();
    Serial.println("Novos dados salvos na memória:");
    Serial.println("Indicativo: " + String(indicativo));
    Serial.println("SSID: " + ssid);
    Serial.println("Símbolo: /" + String(simbolo));
    Serial.println("Mensagem: " + String(mensagem));
  }

  server.sendHeader("Location","/salvo");
  server.send(303);
}

void salvarDados(){
  Serial.println("Salvando dados");
  preferences.putString("indicativo", indicativo);
  preferences.putInt("ssid", ssid);
  preferences.putChar("simbolo", simbolo);
  preferences.putInt("intervalo_tx", intervalo_de_tx);
  preferences.putString("mensagem", mensagem);
}

void handleSalvo(){
  String html = "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "<meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>Configuração ESP32</title>"
                "</head>"
                "<body>"
                "<h1>Configuração Salva</h1>"
                "<h2><a href='/desligar'>Desligar Servidor Web</a><h2>"
                "<h2><a href='/'>Voltar à página principal</a></h2>"
                "<footer>"
                "<h4>Criado por PY4RDG</h4>"
                "<h4>py4rdg@gmail.com</h4>"
                "</footer>"
                "</body>"
                "</html>";
  server.send(200, "text/html", html);
}

void handleDesligar(){
 
  preferences.end();

  //Envia para a pagina salva e desliga o wifi e o access point
  server.sendHeader("Location","/salvo");
  server.send(303);
  delay(1000);
  
  // Desconecta e desativa o Access Point
  WiFi.softAPdisconnect(true);
  
  // Desliga completamente o Wi-Fi
  WiFi.mode(WIFI_OFF);

  Serial.println("Access Point desativado e Wi-Fi desligado.");

  vTaskDelete(NULL); //Deleta a própria task
}

void abrirDadosSalvos(){
    // Carrega os dados salvos nas preferências
    String ind = preferences.getString("indicativo");
    strncpy(indicativo, ind.c_str(), sizeof(indicativo)-1);
    indicativo[sizeof(indicativo)-1] = '\0';
    
    ssid = preferences.getInt("ssid");
    
    simbolo = preferences.getChar("simbolo");
    
    String msg = preferences.getString("mensagem");
    strncpy(mensagem, msg.c_str(), sizeof(mensagem)-1);
    mensagem[sizeof(mensagem)-1] = '\0';

    intervalo_de_tx = preferences.getInt("intervalo_tx");
    
    // Log dos dados carregados
    Serial.println("Dados carregados da memória:");
    Serial.print("Indicativo: ");
    Serial.println(indicativo);
    Serial.println("SSID: " + String(ssid));
    Serial.print("Simbolo: /");
    Serial.println(simbolo);
    Serial.print("Intervalo de TX: ");
    Serial.println(intervalo_de_tx);
    Serial.print("Mensagem: ");
    Serial.println(mensagem);
}
