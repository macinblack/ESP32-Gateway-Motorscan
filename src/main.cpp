#include <stdlib.h>
#include <WiFi.h>
#include <esp_now.h>
//#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

//WiFiMulti wifiMulti;
#define DEVICE "ESP32"

// Credenciais do Router
// WiFi AP SSID
#define WIFI_SSID "NOS-B716"
// WiFi password
#define WIFI_PASSWORD "naovaisdescobrir"

// Credenciais do ponto de acesso (Sensor) 
#define SSID_AP "ESP32-Access-Point"
#define PASSWORD_AP "123456789"

// Canal Wi-Fi channel para o access point 
#define CHAN_AP 2

// Servidor (URL) InfluxDB v2 , e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "http://192.168.1.18:8086"
// token da API do servidor InfluxDB v2  (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "cMwBJ1XApu_O7-M_rPXLodqozRGMKIYHMDCOykL9NW4Wd4x9yTUdFXMzcC7B8TWQclB2C62nSExPhFCXmRuOdA=="
// Nome da organização InfluxDB v2 (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "ISEP"
// Nome do Bucket InfluxDB v2 (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "PESTA"

// Certificado cliente do InfluxDB 
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Fusos horários de acordo com https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examplos:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// Exemplo de estutura para receber os dados enviados pelo sensor MotorScan
typedef struct struct_message {
  int id; //ID do sensor
  float temp; //Temperatura
  float pressure; //Pressão
  float altitude; //Altitude
  float humidity; //Humidade
  float accel_X; //Acelerómetro X
  float accel_Y; //Acelerómetro Y
  float accel_Z; // Acelerómetro Z
}struct_message;

// Criação da struct_message chamada myData
struct_message myData;

// Criação de structure para guardar os dados de cada sensor
struct_message board1;
struct_message board2;

// Criação de um vetor para as estruturas dos diversos sensores (Neste caso apenas para 2 sensores)
struct_message boardsStruct[2] = {board1, board2};

// Apontador de dados
Point sensor("wifi_status");

void timeSync() {
  // Sincroniza com os servidores
  // É necessária precisão no tempo para validar o envido dos dados no servidor
  configTime(0, 0, "pool.ntp.org", "time.nis.gov");
  
  // Difinir Fuso horário
  setenv("TZ", TZ_INFO, 1);

  // Aguarda até a hora estar sincronizada
  Serial.print("Sincronizando o tempo");
  int i = 0;
  while (time(nullptr) < 1000000000ul && i < 100) {
    Serial.print(".");
    delay(100);
    i++;
  }
  Serial.println();

  // Mostra a hora na consola
  time_t tnow = time(nullptr);
  Serial.print("Tempo Sincronizado: ");
  Serial.println(String(ctime(&tnow)));
}


// função callback que será executada quando forem recebidos dados 
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  Serial.print("Pacotes recebidos do Mac Address: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.printf("ID da Placa %u: %u bytes\n", myData.id, len);

  // Atualização das estruturas para os novos dados recebidos
  boardsStruct[myData.id-1].temp = myData.temp;
  boardsStruct[myData.id-1].pressure = myData.pressure;
  boardsStruct[myData.id-1].altitude = myData.altitude;
  boardsStruct[myData.id-1].humidity = myData.humidity;
  boardsStruct[myData.id-1].accel_X = myData.accel_X;
  boardsStruct[myData.id-1].accel_Y = myData.accel_Y;
  boardsStruct[myData.id-1].accel_Z = myData.accel_Z;
  Serial.printf("Temperatura: %0.2f ºC \n", boardsStruct[myData.id-1].temp);
  Serial.printf("Pressão: %0.2f hPa\n", boardsStruct[myData.id-1].pressure);
  Serial.printf("Altitude: %0.2f m\n", boardsStruct[myData.id-1].altitude);
  Serial.printf("Humidade: %0.2f %%\n", boardsStruct[myData.id-1].humidity);
  Serial.printf("Acel. Eixo Radial X: %0.2f \n", boardsStruct[myData.id-1].accel_X);
  Serial.printf("Acel. Eixo Radial Y: %0.2f \n", boardsStruct[myData.id-1].accel_Y);
  Serial.printf("Acel. Eixo Axial A: %0.2f \n", boardsStruct[myData.id-1].accel_Z);
  Serial.println();
}

void setup() {
  //Iniciação das comunicações (Baud Rate=115200)
  Serial.begin(115200);
  Serial.println();
  Serial.print("MAC ADDRESS do ESP32:  ");
  Serial.println(WiFi.macAddress());

    // Definir Wi-Fi (Access Point + Station)
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Conectando ao WiFi do Router\n");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\n");
  Serial.print("Gateway conectado ao router");
  Serial.println();


  Serial.print("Endereço IP da Estação: ");
  Serial.println(WiFi.localIP());
  Serial.print("Canal Wi-Fi: ");
  Serial.println(WiFi.channel());

   WiFi.softAP(SSID_AP, PASSWORD_AP, CHAN_AP, true); 

    // Adicionar TAGs
  sensor.addTag("Dispositivo", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  // Sincronismo de tempo para validação
  timeSync();

  // Verificar a conexão ao servidor
  if (client.validateConnection()) {
    Serial.print("Conectado ao InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("Falhou a conexão ao InfluxDB: ");
    Serial.println(client.getLastErrorMessage()); 
  }
  //Inicia o protocolo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar o ESP-NOW");
    return;
  }
  
  // Uma vez o ESP-NOW inicidado com sucesso, existirá um registo para enviar um callback para obter o estado dos pacotes recebidos
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
    // Apagar todos os dados dos campos
  sensor.clearFields();
  // Leitura dos dados vindo da estrutura do sensor
  sensor.addField("Temperatura", boardsStruct[myData.id-1].temp);
  sensor.addField("Pressão", boardsStruct[myData.id-1].pressure);
  sensor.addField("Altitude", boardsStruct[myData.id-1].altitude);
  sensor.addField("Humidade", boardsStruct[myData.id-1].humidity);
  sensor.addField("Acel. Eixo X", boardsStruct[myData.id-1].accel_X);
  sensor.addField("Acel. Eixo Y", boardsStruct[myData.id-1].accel_Y);
  sensor.addField("Acel. Eixo Z", boardsStruct[myData.id-1].accel_Z);

  
  Serial.print("Escrevendo: ");
  Serial.println(sensor.toLineProtocol());
  // Se não existir conexão Wi-Fi, tenta reconectar
  if ((WiFi.RSSI() == 0) && (WiFi.status() != WL_CONNECTED)){
    Serial.println("Conexão WiFi Perdida");
    }
  // Informa que falhou o envio dos dados
  if (!client.writePoint(sensor)) {
    Serial.print("Falhou envio de dados para o InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  }

  //Aguarda 10 segundos
  Serial.println("Espere 10s");
  delay(10000); 
}