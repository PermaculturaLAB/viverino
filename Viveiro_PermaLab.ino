/*
Sistema de rega e monitoramento automatizado desenvolvido para instalação em hortas residenciais e sistemas agroflorestais de
pequeno e médio porte
*/

#include "DHT.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"
#include "SD.h"
#define DHTTYPE DHT22
#define ECHO_TO_SERIAL 1         //Envia registro de dados para serial se 1, nada se 0
#define LOG_INTERVAL 10000     //Intervado de medidas (6 minutes = 360000)

#define TempsoloPin 1      //output do sensor de temperatura
#define UmidadesoloPin A2  //output do sensor de umidade
#define LuzsolPin 0      //output sensor de luz
#define dhtPin 2           // output sensor DHT
#define chipSelect 10       //output da placa rtc/sd (infos medidas pelos sensores)
#define LEDPinVerde 6       //output LED verde
#define LEDPinVermelho 7         //output LED vermelha
#define solenoidePin 8       //output solenoide
const int TempoRega = 600000; //definir tempo de rega (10 min para iniciar)
const float LimiteRega = 15; //aciona o sistema em valores abaixo desse valor

DHT dht(dhtPin, DHTTYPE);
RTC_DS1307 rtc;

float TempSolo = 0;           //valor escalar da temperatura do solo (degrees F)
float UmidSoloBrut = 0;    //valor bruto do input do sensor de umidade do solo (volts)
float UmidSolo = 0;       //Svalor escalado da contedo volumtrico de agua no solo (percent)
float Umidade = 0;           //umidade relativa (%)
float TempAr = 0;            //temperatura do ar (degrees F)
float IndexCalor = 0;          //indice de calor (degrees F)
float LuzSol = 0;          //luz solar em lux
bool rega = false;        
bool regadoHoje = false;    
DateTime now;
File logfile;

/*
Referencia para umidade no solo

Ar = 0%
Solo muito seco = 10%
Possivelmente abaixo do que voc gostaria = 20%
Bem irrigado = 50%
Copo de agua = 100%
*/


void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);
  
  // LED vermelho indicando erro
  digitalWrite(LEDPinVermelho, HIGH);
  
  while(1);
}

void setup() {
  
  //Iniciar conexcao serial
  Serial.begin(9600); //apenas para teste
  Serial.println("Iniciando SD...");
  
  
  pinMode(chipSelect, OUTPUT); //Pin para gravar no cartao SD
  pinMode(LEDPinVerde, OUTPUT); //pin da LED verde
  pinMode(LEDPinVermelho, OUTPUT); //pin da LED vermelha
  pinMode(solenoidePin, OUTPUT); //pin do solenoide
  digitalWrite(solenoidePin, LOW); //certifique-se que a valvula esta deligada (LOW)
  analogReference(EXTERNAL); //Define a tensão máxima das entradas analógicas para o que está conectado a saida Aref (deve ser de 3.3v)
  
  //Estabelece conexao com o sensor DHT
  dht.begin();
  
  //estabelece conexao com o relogio em tempo real
  Wire.begin();
  if (!rtc.begin()) {
    logfile.println("RTC falhou");
#if ECHO_TO_SERIAL
    Serial.println("RTC falhou");
#endif  //ECHO_TO_SERIAL
  }
  
  //Define data e hora em tempo real se necessario
  if (! rtc.isrunning()) {
    // Linhas seguintes define a data e a hora do RTC no momento que for compilado
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  //teste se o SD esta presente e inicia
  if (!SD.begin(chipSelect)) {
    error("SD com defeito ou ausente");        
  }
  
  Serial.println("SD iniciado");
  
  // criar novo arquivo
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // apenas abre um novo arquivo se ele nao existe
      logfile = SD.open(filename, FILE_WRITE); 
      break;  // abandonar o loop!
    }
  }
  
  if (! logfile) {
    error("impossvel criar arquivo");
  }
  
  Serial.print("Logging to: ");
  Serial.println(filename);
  
  
  logfile.println("Unix Time (s),Date,Soil Temp (C),Air Temp (C),Soil Moisture Content (%),Relative Humidity (%),Heat Index (C),Sunlight Illumination (lux),Watering?");   //HEADER 
#if ECHO_TO_SERIAL
  Serial.println("Unix Time (s),Date,Soil Temp (C),Air Temp (C),Soil Moisture Content (%),Relative Humidity (%),Heat Index (C),Sunlight Illumination (lux),Watering?");
#endif ECHO_TO_SERIAL// attempt to write out the header to the file

  now = rtc.now();
    
}

void loop() {
    
  //delay software
  delay((LOG_INTERVAL -1) - (millis() % LOG_INTERVAL));
  
  //tres piscadas significa inicio de um novo ciclo
  digitalWrite(LEDPinVerde, HIGH);
  delay(150);
  digitalWrite(LEDPinVerde, LOW);
  delay(150);
  digitalWrite(LEDPinVerde, HIGH);
  delay(150);
  digitalWrite(LEDPinVerde, LOW);
  delay(150);
  digitalWrite(LEDPinVerde, HIGH);
  delay(150);
  digitalWrite(LEDPinVerde, LOW);
  
  //Redefinir variável regadoHoje se for um novo dia
  if (!(now.day()==rtc.now().day())) {
    regadoHoje = false;
  }
  
  now = rtc.now();
  
  // log time
  logfile.print(now.unixtime()); // segundos desde 2000
  logfile.print(",");
  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print(",");
 #if ECHO_TO_SERIAL
  Serial.print(now.unixtime()); // segundos desde 2000
  Serial.print(",");
  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print(",");
#endif //ECHO_TO_SERIAL
  
  //coletar variveis
  TempSolo = (75.006 * analogRead(TempsoloPin)*(3.3 / 1024)) - 42;
  delay(20);
  
  UmidSoloBrut = analogRead(UmidadesoloPin)*(3.3/1024);
  delay(20);
  
  //O conteúdo de água volumétrico é uma função por partes da tensão do sensor
  if (UmidSoloBrut < 1.1) {
    UmidSolo = (10 * UmidSoloBrut) - 1;
  }
  else if (UmidSoloBrut < 1.3) {
    UmidSolo = (25 * UmidSoloBrut) - 17.5;
  }
  else if (UmidSoloBrut < 1.82) {
    UmidSolo = (48.08 * UmidSoloBrut) - 47.5;
  }
  else if (UmidSoloBrut < 2.2) {
    UmidSolo = (26.32 * UmidSoloBrut) - 7.89;
  }
  else {
    UmidSolo = (62.5 * UmidSoloBrut) - 87.5;
  }
    
  Umidade = dht.readHumidity();
  delay(20);
  
  TempAr = dht.readTemperature(true);
  delay(20);
  
  IndexCalor = dht.computeHeatIndex(TempAr,Umidade); //indice  calculado a partir da umidade e temp do ar.
  
  //Esta é uma conversão sem refinamento que tenta calibrar usando uma lanterna de um brilho conhecido (pode ser substituído por sensor UV
  LuzSol = pow(((((150 * 3.3)/(analogRead(LuzsolPin)*(3.3/1024))) - 150) / 70000),-1.25); //esses dados foram inseridos a partir de um exemplo... precisa ser revisto!!
  delay(20);
  
  //Log variables
  logfile.print(TempSolo);
  logfile.print(",");
  logfile.print(TempAr);
  logfile.print(",");
  logfile.print(UmidSolo);
  logfile.print(",");
  logfile.print(Umidade);
  logfile.print(",");
  logfile.print(IndexCalor);
  logfile.print(",");
  logfile.print(LuzSol);
  logfile.print(",");
#if ECHO_TO_SERIAL
  Serial.print(TempSolo);
  Serial.print(",");
  Serial.print(TempAr);
  Serial.print(",");
  Serial.print(UmidSolo);
  Serial.print(",");
  Serial.print(Umidade);
  Serial.print(",");
  Serial.print(IndexCalor);
  Serial.print(",");
  Serial.print(LuzSol);
  Serial.print(",");
#endif
  
  if ((UmidSolo < LimiteRega) && (now.hour() > 19) && (now.hour() < 22) && (regadoHoje = false)) {
    //irrigar o jardim
    digitalWrite(solenoidePin, HIGH);
    delay(TempoRega);
    digitalWrite(solenoidePin, LOW);
  
    //gravar que esta irrigando
    logfile.print("TRUE");
#if ECHO_TO_SERIAL
    Serial.print("TRUE");
#endif
  
    regadoHoje = true;
  }
  else {
    logfile.print("FALSE");
#if ECHO_TO_SERIAL
    Serial.print("FALSE");
#endif
  }
  
  logfile.println();
#if ECHO_TO_SERIAL
  Serial.println();
#endif
  delay(50);
  
  //gravar dados no SD
  logfile.flush();
  delay(5000);
}
