#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <WebSocketsServer.h> 
#include <ArduinoJson.h>      // Adicionada para processar os comandos via WebSocket

// --- CONFIGURAÇÃO DO ACCESS POINT ---
const char* ssid_ap = "PLACAR_MP3";
const char* password_ap = "12345678"; 

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); 

// --- CONFIGURAÇÃO DOS LEDS ---
#define NUM_LEDS          21  
#define LEDS_POR_GRUPO     3  
#define TOTAL_GRUPOS       7  

#define PIN_ESQ_DEZENA    D5  
#define PIN_ESQ_UNIDADE   D2  
#define PIN_DIR_DEZENA    D6  
#define PIN_DIR_UNIDADE   D1  

Adafruit_NeoPixel fitaEsqDezena(NUM_LEDS, PIN_ESQ_DEZENA, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel fitaEsqUnidade(NUM_LEDS, PIN_ESQ_UNIDADE, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel fitaDirDezena(NUM_LEDS, PIN_DIR_DEZENA, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel fitaDirUnidade(NUM_LEDS, PIN_DIR_UNIDADE, NEO_GRB + NEO_KHZ800);

// --- VARIÁVEIS DE JOGO ---
int pontosEsq = 0, pontosDir = 0;
int consecutivasEsq = 0, consecutivasDir = 0;
int ultimoAMarcar = 0; 
bool jogoFinalizado = false;
int vencedor = 0;      

int ultNumEsqDez = -1, ultNumEsqUni = -1;
int ultNumDirDez = -1, ultNumDirUni = -1;
uint32_t ultCorEsqDez = 0, ultCorEsqUni = 0;
uint32_t ultCorDirDez = 0, ultCorDirUni = 0;

unsigned long tempoPiscadaPontos = 0;
bool piscandoEsq = false, piscandoDir = false;
unsigned long tempoMatchPoint = 0;
bool estadoMatchPointPisca = true;
unsigned long tempoArcoIris = 0;
uint16_t pixelHueArcoIris = 0;

bool alguemConectadoAnteriormente = false;

byte calorEsq[NUM_LEDS];
byte calorDir[NUM_LEDS];
unsigned long tempoFogo = 0;

unsigned long tempoPiscaEspera = 0;
bool estadoLedsEspera = false; 
const int ledsParaPiscar[] = {5, 6, 8, 9, 14, 15, 17, 18};
const int qtdLedsParaPiscar = sizeof(ledsParaPiscar) / sizeof(ledsParaPiscar[0]);

unsigned long tempoUltimoBroadcastEfeito = 0;

const byte mapeamentoNumeros[10][TOTAL_GRUPOS] = {
  {0, 1, 1, 1, 1, 1, 1}, {0, 1, 0, 0, 0, 0, 1}, {1, 1, 1, 0, 1, 1, 0},
  {1, 1, 1, 0, 0, 1, 1}, {1, 1, 0, 1, 0, 0, 1}, {1, 0, 1, 1, 0, 1, 1},
  {1, 0, 1, 1, 1, 1, 1}, {0, 1, 1, 0, 0, 0, 1}, {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 0, 1, 1}
};

void transmitirEstadoGeral() {
  bool esqEmMatch = (pontosEsq >= 14 && pontosEsq > pontosDir && !jogoFinalizado);
  bool dirEmMatch = (pontosDir >= 14 && pontosDir > pontosEsq && !jogoFinalizado);
  bool fogoEsqAtivo = (consecutivasEsq >= 3 && !jogoFinalizado);
  bool fogoDirAtivo = (consecutivasDir >= 3 && !jogoFinalizado);

  String json = "{";
  json += "\"esquerda\":{\"pontos\":" + String(pontosEsq) + ",\"fogo\":" + String(fogoEsqAtivo ? "true" : "false") + ",\"match\":" + String(esqEmMatch ? "true" : "false") + "},";
  json += "\"direita\":{\"pontos\":" + String(pontosDir) + ",\"fogo\":" + String(fogoDirAtivo ? "true" : "false") + ",\"match\":" + String(dirEmMatch ? "true" : "false") + "},";
  json += "\"jogoFinalizado\":" + String(jogoFinalizado ? "true" : "false") + ",";
  json += "\"vencedor\":" + String(vencedor);
  json += "}";

  webSocket.broadcastTXT(json);
}

void renderizarDigitoIntel(Adafruit_NeoPixel &fita, int numero, uint32_t cor, int &ultNum, uint32_t &ultCor, bool forcarRefresh = false) {
  if (!forcarRefresh && (numero == ultNum) && (cor == ultCor)) {
    return; 
  }
  
  max(0,1); // Dummy expression safe for compiler
  ultNum = numero;
  ultCor = cor;
  
  fita.clear();
  if (numero >= 0 && numero <= 9) {
    for (int grupo = 0; grupo < TOTAL_GRUPOS; grupo++) {
      if (mapeamentoNumeros[numero][grupo] == 1) {
        int ledInicial = grupo * LEDS_POR_GRUPO; // Note: using grouping calculation
        int ledInicialCalculado = grupo * LEDS_POR_GRUPO;
        int ledFinal = ledInicialCalculado + LEDS_POR_GRUPO;
        for (int i = ledInicialCalculado; i < ledFinal; i++) {
          fita.setPixelColor(i, cor);
        }
      }
    }
  }
  fita.show();
  yield(); 
}

void resetarCacheRender() {
  ultNumEsqDez = -1; ultNumEsqUni = -1;
  ultNumDirDez = -1; ultNumDirUni = -1;
  ultCorEsqDez = 0;  ultCorEsqUni = 0;
  ultCorDirDez = 0;  ultCorDirUni = 0;
}

void executarIntroConexao() {
  uint32_t corVerde = fitaEsqUnidade.Color(0, 255, 0);
  uint32_t corApagada = fitaEsqUnidade.Color(0, 0, 0);

  fitaEsqDezena.setBrightness(45);  fitaEsqUnidade.setBrightness(45);
  fitaDirDezena.setBrightness(45);  fitaDirUnidade.setBrightness(45);

  for (int pisca = 0; pisca < 3; pisca++) {
    renderizarDigitoIntel(fitaEsqDezena, 8, corVerde, ultNumEsqDez, ultCorEsqDez, true);
    renderizarDigitoIntel(fitaEsqUnidade, 8, corVerde, ultNumEsqUni, ultCorEsqUni, true);
    renderizarDigitoIntel(fitaDirDezena, 8, corVerde, ultNumDirDez, ultCorDirDez, true);
    renderizarDigitoIntel(fitaDirUnidade, 8, corVerde, ultNumDirUni, ultCorDirUni, true);
    transmitirEstadoGeral();
    delay(300); ESP.wdtFeed();

    renderizarDigitoIntel(fitaEsqDezena, 8, corApagada, ultNumEsqDez, ultCorEsqDez, true);
    renderizarDigitoIntel(fitaEsqUnidade, 8, corApagada, ultNumEsqUni, ultCorEsqUni, true);
    renderizarDigitoIntel(fitaDirDezena, 8, corApagada, ultNumDirDez, ultCorDirDez, true);
    renderizarDigitoIntel(fitaDirUnidade, 8, corApagada, ultNumDirUni, ultCorDirUni, true);
    transmitirEstadoGeral();
    delay(200); ESP.wdtFeed();
  }
  
  pontosEsq = 0; pontosDir = 0;
  consecutivasEsq = 0; consecutivasDir = 0;
  ultimoAMarcar = 0; jogoFinalizado = false; vencedor = 0;
  resetarCacheRender();
  transmitirEstadoGeral();
}

void rodarAnimacaoEsperaFixa() {
  unsigned long tempoAtual = millis();
  if (tempoAtual - tempoPiscaEspera >= 500) { 
    tempoPiscaEspera = tempoAtual;
    estadoLedsEspera = !estadoLedsEspera;

    fitaEsqDezena.clear();  fitaEsqUnidade.clear();
    fitaDirDezena.clear();  fitaDirUnidade.clear();

    if (estadoLedsEspera) {
      uint32_t corVerdeEspera = fitaEsqUnidade.Color(0, 255, 0);
      for (int i = 0; i < qtdLedsParaPiscar; i++) {
        int idxLed = ledsParaPiscar[i];
        fitaEsqDezena.setPixelColor(idxLed, corVerdeEspera);
        fitaEsqUnidade.setPixelColor(idxLed, corVerdeEspera);
        fitaDirDezena.setPixelColor(idxLed, corVerdeEspera);
        fitaDirUnidade.setPixelColor(idxLed, corVerdeEspera);
      }
    }
    fitaEsqDezena.show();  fitaEsqUnidade.show();
    fitaDirDezena.show();  fitaDirUnidade.show();
    yield(); 
  }
}

uint32_t calcularCorFogo(byte calor) {
  byte r = map(calor, 0, 255, 0, 255);
  byte g = map(calor, 0, 255, 0, 105); 
  return Adafruit_NeoPixel::Color(r, g, 0);
}

void processarFogoNoDigito(Adafruit_NeoPixel &fita, byte *calorArray, int numero) {
  for (int i = 0; i < NUM_LEDS; i++) {
    int decremento = random(0, 16);
    calorArray[i] = (calorArray[i] > decremento) ? calorArray[i] - decremento : 0;
  }
  for (int k = NUM_LEDS - 1; k >= 2; k--) {
    calorArray[k] = (calorArray[k - 1] + calorArray[k - 2] + calorArray[k - 2]) / 3;
  }
  if (random(255) < 85) {
    int m = random(0, 4);
    int incremento = random(160, 256);
    calorArray[m] = (calorArray[m] + incremento > 255) ? 255 : calorArray[m] + incremento;
  }

  fita.clear();
  if (numero >= 0 && numero <= 9) {
    for (int grupo = 0; grupo < TOTAL_GRUPOS; grupo++) {
      if (mapeamentoNumeros[numero][grupo] == 1) {
        int ledInicial = grupo * LEDS_POR_GRUPO;
        int ledFinal = ledInicial + LEDS_POR_GRUPO;
        for (int i = ledInicial; i < ledFinal; i++) {
          fita.setPixelColor(i, calcularCorFogo(calorArray[i]));
        }
      }
    }
  }
  fita.show();
}

void gerenciarEfeitosEVisores() {
  unsigned long tempoAtual = millis();

  if (jogoFinalizado) {
    if (tempoAtual - tempoArcoIris >= 15) {
      tempoArcoIris = tempoAtual;
      pixelHueArcoIris += 256; 
      uint32_t corArcoIris = Adafruit_NeoPixel::ColorHSV(pixelHueArcoIris);
      
      if (vencedor == 1) {
        renderizarDigitoIntel(fitaEsqDezena, pontosEsq / 10, corArcoIris, ultNumEsqDez, ultCorEsqDez, true);
        renderizarDigitoIntel(fitaEsqUnidade, pontosEsq % 10, corArcoIris, ultNumEsqUni, ultCorEsqUni, true);
        fitaDirDezena.clear(); fitaDirUnidade.clear(); fitaDirDezena.show(); fitaDirUnidade.show();
      } else {
        renderizarDigitoIntel(fitaDirDezena, pontosDir / 10, corArcoIris, ultNumDirDez, ultCorDirDez, true);
        renderizarDigitoIntel(fitaDirUnidade, pontosDir % 10, corArcoIris, ultNumDirUni, ultCorDirUni, true);
        fitaEsqDezena.clear(); fitaEsqUnidade.clear(); fitaEsqDezena.show(); fitaEsqUnidade.show();
      }
    }
    return;
  }

  uint32_t corEsq = fitaEsqUnidade.Color(255, 0, 0); 
  uint32_t corDir = fitaDirUnidade.Color(0, 0, 255); 

  bool aplicandoPiscadaEsq = (piscandoEsq && (tempoAtual - tempoPiscadaPontos < 200));
  bool aplicandoPiscadaDir = (piscandoDir && (tempoAtual - tempoPiscadaPontos < 200));

  if (aplicandoPiscadaEsq) corEsq = fitaEsqUnidade.Color(255, 255, 255);
  if (aplicandoPiscadaDir) corDir = fitaDirUnidade.Color(255, 255, 255);

  if (!aplicandoPiscadaEsq) piscandoEsq = false;
  if (!aplicandoPiscadaDir) piscandoDir = false;

  bool esqEmMatch = (pontosEsq >= 14 && pontosEsq > pontosDir);
  bool dirEmMatch = (pontosDir >= 14 && pontosDir > pontosEsq);
  
  bool mudouEstadoPisca = false;
  if (esqEmMatch || dirEmMatch) {
    if (tempoAtual - tempoMatchPoint >= 300) {
      tempoMatchPoint = tempoAtual;
      estadoMatchPointPisca = !estadoMatchPointPisca;
      mudouEstadoPisca = true;
    }
  } else {
    estadoMatchPointPisca = true;
  }

  bool fogoEsqAtivo = (consecutivasEsq >= 3 && !aplicandoPiscadaEsq);
  bool fogoDirAtivo = (consecutivasDir >= 3 && !aplicandoPiscadaDir);

  if (mudouEstadoPisca && (tempoAtual - tempoUltimoBroadcastEfeito > 100)) {
    tempoUltimoBroadcastEfeito = tempoAtual;
    transmitirEstadoGeral();
  }

  if (esqEmMatch && !estadoMatchPointPisca && !aplicandoPiscadaEsq) {
    fitaEsqDezena.clear(); fitaEsqUnidade.clear();
    fitaEsqDezena.show();  fitaEsqUnidade.show();
    ultNumEsqDez = -1;     ultNumEsqUni = -1; 
  } 
  else if (fogoEsqAtivo) {
    if (tempoAtual - tempoFogo >= 35) {
      processarFogoNoDigito(fitaEsqDezena, calorEsq, pontosEsq / 10);
      processarFogoNoDigito(fitaEsqUnidade, calorEsq, pontosEsq % 10);
    }
    ultNumEsqDez = -1; ultNumEsqUni = -1;
  } 
  else {
    renderizarDigitoIntel(fitaEsqDezena, pontosEsq / 10, corEsq, ultNumEsqDez, ultCorEsqDez);
    renderizarDigitoIntel(fitaEsqUnidade, pontosEsq % 10, corEsq, ultNumEsqUni, ultCorEsqUni);
  }

  if (dirEmMatch && !estadoMatchPointPisca && !aplicandoPiscadaDir) {
    fitaDirDezena.clear(); fitaDirUnidade.clear();
    fitaDirDezena.show();  fitaDirUnidade.show();
    ultNumDirDez = -1;     ultNumDirUni = -1; 
  } 
  else if (fogoDirAtivo) {
    if (tempoAtual - tempoFogo >= 35) {
      processarFogoNoDigito(fitaDirDezena, calorDir, pontosDir / 10);
      processarFogoNoDigito(fitaDirUnidade, calorDir, pontosDir % 10);
    }
    ultNumDirDez = -1; ultNumDirUni = -1;
  } 
  else {
    renderizarDigitoIntel(fitaDirDezena, pontosDir / 10, corDir, ultNumDirDez, ultCorDirDez);
    renderizarDigitoIntel(fitaDirUnidade, pontosDir % 10, corDir, ultNumDirUni, ultCorDirUni);
  }

  if (fogoEsqAtivo || fogoDirAtivo) {
    if (tempoAtual - tempoFogo >= 35) {
      tempoFogo = tempoAtual;
    }
  }
}

void checarRegrasDeVitoria() {
  if (pontosEsq >= 15 && (pontosEsq - pontosDir) >= 2) { jogoFinalizado = true; vencedor = 1; } 
  else if (pontosDir >= 15 && (pontosDir - pontosEsq) >= 2) { jogoFinalizado = true; vencedor = 2; }
}

// Lógica de processamento centralizada (Chamada internamente pelo WebSocket ou HTTP)
void executarComandoLozico(String lado, String acao) {
  if (jogoFinalizado && lado != "reset") return;
  unsigned long tempoAtual = millis();

  if (lado == "esq") {
    if (acao == "mais" && pontosEsq < 99) {
      pontosEsq++; piscandoEsq = true; tempoPiscadaPontos = tempoAtual;
      if (ultimoAMarcar == 1) consecutivasEsq++; else { consecutivasEsq = 1; ultimoAMarcar = 1; }
      consecutivasDir = 0;
    } else if (acao == "menos" && pontosEsq > 0) {
      pontosEsq--; consecutivasEsq = 0; if (ultimoAMarcar == 1) ultimoAMarcar = 0;
    }
  } 
  else if (lado == "dir") {
    if (acao == "mais" && pontosDir < 99) {
      pontosDir++; piscandoDir = true; tempoPiscadaPontos = tempoAtual;
      if (ultimoAMarcar == 2) consecutivasDir++; else { consecutivasDir = 1; ultimoAMarcar = 2; }
      consecutivasEsq = 0;
    } else if (acao == "menos" && pontosDir > 0) {
      pontosDir--; consecutivasDir = 0; if (ultimoAMarcar == 2) ultimoAMarcar = 0;
    }
  }
  else if (lado == "reset") {
    pontosEsq = 0; pontosDir = 0; consecutivasEsq = 0; consecutivasDir = 0; ultimoAMarcar = 0; jogoFinalizado = false; vencedor = 0;
    resetarCacheRender();
  }
  checarRegrasDeVitoria();
  transmitirEstadoGeral();
}

void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  transmitirEstadoGeral();
  server.send(200, "application/json", "{\"status\":\"OK\"}");
}

void handleControle() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("lado") || !server.hasArg("acao")) { server.send(400, "text/plain", "Erro"); return; }
  executarComandoLozico(server.arg("lado"), server.arg("acao"));
  server.send(200, "text/plain", "OK");
}

// Manipulador unificado de eventos WebSocket (Lê os comandos JSON do PWA)
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      transmitirEstadoGeral(); 
      break;
      
    case WStype_TEXT: {
      StaticJsonDocument<200> doc;
      DeserializationError erro = deserializeJson(doc, payload);
      
      if (!erro && doc.containsKey("comando")) {
        String lado = doc["lado"];
        String acao = doc["acao"];
        executarComandoLozico(lado, acao);
      }
      break;
    }
    default:
      break;
  }
}

void setup() {
  WiFi.persistent(false); 
  WiFi.disconnect(true);
  
  fitaEsqDezena.begin();  fitaEsqDezena.setBrightness(45);
  fitaEsqUnidade.begin(); fitaEsqUnidade.setBrightness(45);
  fitaDirDezena.begin();  fitaDirDezena.setBrightness(45);
  fitaDirUnidade.begin(); fitaDirUnidade.setBrightness(45);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid_ap, password_ap, 1, 0, 1); 

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/controlar", HTTP_GET, handleControle);
  server.begin();
}

void loop() {
  webSocket.loop(); 
  server.handleClient();

  int dispositivosConectados = WiFi.softAPgetStationNum();

  if (dispositivosConectados == 0) {
    alguemConectadoAnteriormente = false;
    rodarAnimacaoEsperaFixa();
  } 
  else {
    if (!alguemConectadoAnteriormente) {
      delay(10);
      executarIntroConexao(); 
      alguemConectadoAnteriormente = true;
    }
    gerenciarEfeitosEVisores();
  }
  delay(1); 
}