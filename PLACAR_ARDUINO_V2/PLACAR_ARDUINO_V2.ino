#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <WebSocketsServer.h> 
#include <ArduinoJson.h>      

// --- CONFIGURAÇÃO DO ACCESS POINT ---
const char* ssid_ap = "PLACAR_MP3";
const char* password_ap = "12345678"; 

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); 

// --- CONFIGURAÇÃO DOS LEDS (BARRAMENTO ÚNICO) ---
#define PIN_LEDS          D1 
#define LEDS_POR_DIGITO   21  
#define LEDS_POR_GRUPO     3  
#define TOTAL_GRUPOS       7  
#define TOTAL_LEDS        84 

Adafruit_NeoPixel fita(TOTAL_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

// Índices de início (Offsets) para cada dígito
#define OFFSET_ESQ_DEZENA   0
#define OFFSET_ESQ_UNIDADE  21
#define OFFSET_DIR_DEZENA   42
#define OFFSET_DIR_UNIDADE  63

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

// Controle de reinicialização preventiva do rádio
unsigned long tempoSemDispositivos = 0;
bool contandoTempoResetRede = false;
const unsigned long TIMEOUT_RESET_REDE = 60000; 

byte calorEsq[LEDS_POR_DIGITO]; 
byte calorDir[LEDS_POR_DIGITO]; 
unsigned long tempoFogo = 0;

// --- CONFIGURAÇÃO DA ANIMAÇÃO DE ESPERA (SNAKE UNIFICADA) ---
const int sequenciaSnake[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 20, 19, 18, 17, 16, 15, 14, 12, 0, 1, 2, 
  21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 21, 22, 23, 41, 40, 39, 38, 37, 36, 35, 34, 33, 21, 22, 23,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 42, 43, 44, 62, 61, 60, 59, 58, 57, 56, 55, 54, 42, 43, 44,
  63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 63, 64, 65, 83, 82, 81, 80, 79, 78, 77, 76, 75, 63, 64, 65
};
const int tamanhoSnakeSeq = sizeof(sequenciaSnake) / sizeof(sequenciaSnake[0]);
const int comprimentoSnakeBody = 4; 

unsigned long tempoSnakeAtualizacao = 0;
int frameSnake = 0;

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

void reiniciarAccessPoint() {
  WiFi.softAPdisconnect(false); 
  delay(50);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid_ap, password_ap, 1, 0, 1);
}

void limparDigitoEspecifico(int offset) {
  for (int i = 0; i < LEDS_POR_DIGITO; i++) {
    fita.setPixelColor(offset + i, 0);
  }
}

void renderizarDigitoIntel(int offset, int numero, uint32_t cor, int &ultNum, uint32_t &ultCor, bool forcarRefresh = false) {
  if (!forcarRefresh && (numero == ultNum) && (cor == ultCor)) {
    return; 
  }
  
  ultNum = numero;
  ultCor = cor;
  
  limparDigitoEspecifico(offset);
  if (numero >= 0 && numero <= 9) {
    for (int grupo = 0; grupo < TOTAL_GRUPOS; grupo++) {
      if (mapeamentoNumeros[numero][grupo] == 1) {
        int ledInicialCalculado = grupo * LEDS_POR_GRUPO;
        int ledFinal = ledInicialCalculado + LEDS_POR_GRUPO;
        for (int i = ledInicialCalculado; i < ledFinal; i++) {
          fita.setPixelColor(offset + i, cor);
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

void executarEfeitoFogos() {
  uint32_t corBranca = fita.Color(255, 255, 255);
  
  const int estopinEsq[] = {23, 22, 21, 2, 1, 0};
  const int estopinDir[] = {42, 43, 44, 63, 64, 65};
  const int passosEstopin = 6;

  for (int i = 0; i < passosEstopin; i++) {
    fita.clear();
    fita.setPixelColor(estopinEsq[i], corBranca);
    fita.setPixelColor(estopinDir[i], corBranca);
    fita.show();
    delay(45); 
    ESP.wdtFeed();
  }

  for (int estouro = 0; estouro < 3; estouro++) {
    fita.fill(corBranca, 0, TOTAL_LEDS);
    fita.show();
    delay(50);
    ESP.wdtFeed();

    fita.clear();
    fita.show();
    delay(40);
    ESP.wdtFeed();
  }
  
  resetarCacheRender();
}

void executarIntroConexao() {
  uint32_t corVerde = fita.Color(0, 255, 0);
  uint32_t corApagada = fita.Color(0, 0, 0);

  fita.setBrightness(45);

  for (int pisca = 0; pisca < 3; pisca++) {
    renderizarDigitoIntel(OFFSET_ESQ_DEZENA,  8, corVerde, ultNumEsqDez, ultCorEsqDez, true);
    renderizarDigitoIntel(OFFSET_ESQ_UNIDADE, 8, corVerde, ultNumEsqUni, ultCorEsqUni, true);
    renderizarDigitoIntel(OFFSET_DIR_DEZENA,  8, corVerde, ultNumDirDez, ultCorDirDez, true);
    renderizarDigitoIntel(OFFSET_DIR_UNIDADE, 8, corVerde, ultNumDirUni, ultCorDirUni, true);
    transmitirEstadoGeral();
    delay(300); ESP.wdtFeed();

    renderizarDigitoIntel(OFFSET_ESQ_DEZENA,  8, corApagada, ultNumEsqDez, ultCorEsqDez, true);
    renderizarDigitoIntel(OFFSET_ESQ_UNIDADE, 8, corApagada, ultNumEsqUni, ultCorEsqUni, true);
    renderizarDigitoIntel(OFFSET_DIR_DEZENA,  8, corApagada, ultNumDirDez, ultCorDirDez, true);
    renderizarDigitoIntel(OFFSET_DIR_UNIDADE, 8, corApagada, ultNumDirUni, ultCorDirUni, true);
    transmitirEstadoGeral();
    delay(200); ESP.wdtFeed();
  }
  
  pontosEsq = 0; pontosDir = 0;
  consecutivasEsq = 0; consecutivasDir = 0;
  ultimoAMarcar = 0; jogoFinalizado = false; vencedor = 0;
  resetarCacheRender();
  transmitirEstadoGeral();
}

void rodarAnimacaoSnake() {
  unsigned long tempoAtual = millis();
  const int velocidadeSnake = 60; 
  
  if (tempoAtual - tempoSnakeAtualizacao >= velocidadeSnake) {
    tempoSnakeAtualizacao = tempoAtual;
    uint32_t corSnakeVerde = Adafruit_NeoPixel::Color(0, 255, 0);

    fita.clear();

    for (int i = 0; i < comprimentoSnakeBody; i++) {
      int idxNaSeq = (frameSnake + i) % tamanhoSnakeSeq;
      int ledFisicoGlobal = sequenciaSnake[idxNaSeq];
      fita.setPixelColor(ledFisicoGlobal, corSnakeVerde);
    }
    
    fita.show();
    frameSnake = (frameSnake + 1) % tamanhoSnakeSeq;
    yield(); 
  }
}

// Fogo Lado Esquerdo (Agora Azul/Ciano)
uint32_t calcularCorFogo(byte calor) {
  byte r = 0; 
  byte g = map(calor, 0, 255, 0, 160); 
  byte b = map(calor, 0, 255, 45, 255); 
  return Adafruit_NeoPixel::Color(r, g, b);
}

// Fogo Lado Direito (Agora Vermelho/Laranja)
uint32_t calcularCorFogoDir(byte calor) {
  byte r = map(calor, 0, 255, 0, 255);
  byte g = map(calor, 0, 255, 0, 105); 
  return Adafruit_NeoPixel::Color(r, g, 0);
}

void processarFogoNoDigito(int offset, byte *calorArray, int numero, bool isLadoEsquerdo) {
  for (int i = 0; i < LEDS_POR_DIGITO; i++) {
    int decremento = random(0, 16);
    calorArray[i] = (calorArray[i] > decremento) ? calorArray[i] - decremento : 0;
  }
  for (int k = LEDS_POR_DIGITO - 1; k >= 2; k--) {
    calorArray[k] = (calorArray[k - 1] + calorArray[k - 2] + calorArray[k - 2]) / 3;
  }
  if (random(255) < 85) {
    int m = random(0, 4);
    int incremento = random(160, 256);
    calorArray[m] = (calorArray[m] + incremento > 255) ? 255 : calorArray[m] + incremento;
  }

  limparDigitoEspecifico(offset);
  if (numero >= 0 && numero <= 9) {
    for (int grupo = 0; grupo < TOTAL_GRUPOS; grupo++) {
      if (mapeamentoNumeros[numero][grupo] == 1) {
        int ledInicial = grupo * LEDS_POR_GRUPO;
        int ledFinal = ledInicial + LEDS_POR_GRUPO;
        for (int i = ledInicial; i < ledFinal; i++) {
          uint32_t corFogo = isLadoEsquerdo ? calcularCorFogo(calorArray[i]) : calcularCorFogoDir(calorArray[i]);
          fita.setPixelColor(offset + i, corFogo);
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
        renderizarDigitoIntel(OFFSET_ESQ_DEZENA, pontosEsq / 10, corArcoIris, ultNumEsqDez, ultCorEsqDez, true);
        renderizarDigitoIntel(OFFSET_ESQ_UNIDADE, pontosEsq % 10, corArcoIris, ultNumEsqUni, ultCorEsqUni, true);
        limparDigitoEspecifico(OFFSET_DIR_DEZENA); limparDigitoEspecifico(OFFSET_DIR_UNIDADE);
        fita.show();
      } else {
        renderizarDigitoIntel(OFFSET_DIR_DEZENA, pontosDir / 10, corArcoIris, ultNumDirDez, ultCorDirDez, true);
        renderizarDigitoIntel(OFFSET_DIR_UNIDADE, pontosDir % 10, corArcoIris, ultNumDirUni, ultCorDirUni, true);
        limparDigitoEspecifico(OFFSET_ESQ_DEZENA); limparDigitoEspecifico(OFFSET_ESQ_UNIDADE);
        fita.show();
      }
    }
    return;
  }

  // --- INVERSÃO DAS CORES PADRÃO ---
  uint32_t corEsq = fita.Color(0, 0, 255); // Esquerda virou Azul
  uint32_t corDir = fita.Color(255, 0, 0); // Direita virou Vermelho

  bool aplicandoPiscadaEsq = (piscandoEsq && (tempoAtual - tempoPiscadaPontos < 200));
  bool aplicandoPiscadaDir = (piscandoDir && (tempoAtual - tempoPiscadaPontos < 200));

  if (aplicandoPiscadaEsq) corEsq = fita.Color(255, 255, 255);
  if (aplicandoPiscadaDir) corDir = fita.Color(255, 255, 255);

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

  // --- RENDERIZAÇÃO LADO ESQUERDO ---
  if (esqEmMatch && !estadoMatchPointPisca && !aplicandoPiscadaEsq) {
    limparDigitoEspecifico(OFFSET_ESQ_DEZENA); limparDigitoEspecifico(OFFSET_ESQ_UNIDADE);
    fita.show();
    ultNumEsqDez = -1; ultNumEsqUni = -1; 
  } 
  else if (fogoEsqAtivo) {
    if (tempoAtual - tempoFogo >= 35) {
      processarFogoNoDigito(OFFSET_ESQ_DEZENA,  calorEsq, pontosEsq / 10, true);
      processarFogoNoDigito(OFFSET_ESQ_UNIDADE, calorEsq, pontosEsq % 10, true);
    }
    ultNumEsqDez = -1; ultNumEsqUni = -1;
  } 
  else {
    renderizarDigitoIntel(OFFSET_ESQ_DEZENA,  pontosEsq / 10, corEsq, ultNumEsqDez, ultCorEsqDez);
    renderizarDigitoIntel(OFFSET_ESQ_UNIDADE, pontosEsq % 10, corEsq, ultNumEsqUni, ultCorEsqUni);
  }

  // --- RENDERIZAÇÃO LADO DIREITO ---
  if (dirEmMatch && !estadoMatchPointPisca && !aplicandoPiscadaDir) {
    limparDigitoEspecifico(OFFSET_DIR_DEZENA); limparDigitoEspecifico(OFFSET_DIR_UNIDADE);
    fita.show();
    ultNumDirDez = -1; ultNumDirUni = -1; 
  } 
  else if (fogoDirAtivo) {
    if (tempoAtual - tempoFogo >= 35) {
      processarFogoNoDigito(OFFSET_DIR_DEZENA,  calorDir, pontosDir / 10, false);
      processarFogoNoDigito(OFFSET_DIR_UNIDADE, calorDir, pontosDir % 10, false);
    }
    ultNumDirDez = -1; ultNumDirUni = -1;
  } 
  else {
    renderizarDigitoIntel(OFFSET_DIR_DEZENA,  pontosDir / 10, corDir, ultNumDirDez, ultCorDirDez);
    renderizarDigitoIntel(OFFSET_DIR_UNIDADE, pontosDir % 10, corDir, ultNumDirUni, ultCorDirUni);
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

void executarComandoLogico(String lado, String acao) {
  if (jogoFinalizado && lado != "reset") return;
  unsigned long tempoAtual = millis();

  int placarAnteriorEsq = pontosEsq;
  int placarAnteriorDir = pontosDir;

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

  if (pontosEsq == pontosDir && pontosEsq > 0 && (placarAnteriorEsq != pontosEsq || placarAnteriorDir != pontosDir)) {
    executarEfeitoFogos();
  }
}

void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  
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

  server.send(200, "application/json", json);
}

void handleControle() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  
  if (!server.hasArg("lado") || !server.hasArg("acao")) { 
    server.send(400, "text/plain", "Erro: Argumentos ausentes"); 
    return; 
  }

  executarComandoLogico(server.arg("lado"), server.arg("acao"));

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

  server.send(200, "application/json", json);
}

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
        executarComandoLogico(lado, acao);
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
  
  fita.begin();  
  fita.setBrightness(80);
  fita.show(); 

  reiniciarAccessPoint();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/controlar", HTTP_GET, handleControle);
  server.on("/controlar", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(200, "text/plain", "");
  });
  server.begin();
}

void loop() {
  webSocket.loop(); 
  server.handleClient();

  int dispositivosConectados = WiFi.softAPgetStationNum();

  if (dispositivosConectados == 0) {
    alguemConectadoAnteriormente = false;
    rodarAnimacaoSnake();

    if (!contandoTempoResetRede) {
      tempoSemDispositivos = millis();
      contandoTempoResetRede = true;
    } 
    else if (millis() - tempoSemDispositivos >= TIMEOUT_RESET_REDE) {
      reiniciarAccessPoint();
      tempoSemDispositivos = millis(); 
    }
  } 
  else {
    contandoTempoResetRede = false;
    
    if (!alguemConectadoAnteriormente) {
      delay(10);
      executarIntroConexao(); 
      alguemConectadoAnteriormente = true;
    }
    gerenciarEfeitosEVisores();
  }
  delay(1); 
}