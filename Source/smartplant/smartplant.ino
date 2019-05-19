//Librerias
#include <ThingSpeak.h>
#include <ESP8266WiFi.h>

//Etiquetado de pines
#define SOIL A0 //Etiqueto sensor de humedad en el pin A0
#define LED D0  //Etiqueto LED en el pin D0
#define WVALVEPWM D1  //Valvula de riego potencia
#define WVALVEDIR D3  //Valvula de riego sentido de giro

//Constantes
#define ADCWAT 426  //Medicion ADC con agua
#define ADCAIR 774  //Medicion ADC con aire
#define SMAIR 0 //Porcentaje de humedad minimo
#define SMWAT 100 //Porcentaje de humedad maximo
#define MAXREADS 24 //Máximo de muestras para suavizar
#define MUESTREO 1000 //Tiempo de muestreo cada 1000 milisecs
#define TPOST 15000 //Tiempo de envio de datos cada 15000 milisecs
#define SSIDNAME "ARRIS"  //SSID Name
#define SSIDPASS "Tony050794" //SSID Pass
#define TSKEY "7WFD9WNB5S68KGB6"  //Thingspeak API Key write  
#define CHANNUM "0000000" //Thingspeak Channel Number

//Variables
float m = 0;  //Variable para almacenar la humedad del suelo
float em = 0; //Variable para almacenar el error de humedad del suelo
float spm = 60; //Variable para almacenar el "Setpoint" o valor deseado de humedad en 60%
unsigned long tprev = 0;  //Variable para temporizar
unsigned long tprevtx = 0;  //Variable para temporizar internet
unsigned int inputm = 0;  //Variable para filtrar
unsigned int wvalstate = 0; //Variable para indicar el estado actual de la valvula de riego
//->Variables para suavizar
int readingsm[MAXREADS] = {0};
int readIndexm = 0;
long totalm = 0;
//->Variables IoT
int status = WL_IDLE_STATUS;
WiFiClient client;

//Subrutinas y/o funciones
float flmap(float x, float in_min, float in_max, float out_min, float out_max) {
  return constrain((float)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min), out_min, out_max);
}

unsigned long smooth(int pin, long &total, int *readings, int &readIndex) {
  total = total - readings[readIndex];  //Eliminar ultima muestra
  readings[readIndex] = analogRead(pin);  //Leer pin analogo
  total = total + readings[readIndex]; //Sumar la última muestra al total
  readIndex = readIndex + 1; //Avanzar a la siguiente posición del vector

  if (readIndex >= MAXREADS) { //Si estamos en la ultima posicion del vector
    readIndex = 0;  //Empezar otra vez desde el principio del vector
  }
  return total / MAXREADS; //Calcular el promedio
}

void MeasInitialize() { //Inicializar suavizado
  for (unsigned int i = 0; i < 80; i++) {
    inputm = smooth(SOIL, totalm, readingsm, readIndexm);
  }
  m = flmap(inputm, ADCAIR, ADCWAT, SMAIR, SMWAT);
}

void shumidctrl() { //Soil Humidity Controller
  em = spm - m;
  if (em < -3) {  //If the soil moisture is above set point (neg error), turn off water valve
    digitalWrite(WVALVEPWM, LOW); //Apago potencia de la válvula
    wvalstate = 0;
  }
  else if (em > 3) { //else if the soil moisture is below set point (pos error), turn on water valve
    digitalWrite(WVALVEPWM, HIGH); //Prendo potencia de la válvula
    wvalstate = 1;
  }
  else { //if the error is between -10% and 10%, keep the water valve off
    digitalWrite(WVALVEPWM, LOW); //Apago potencia de la válvula
    wvalstate = 0;
  }
}

void printWifiStatus() {
  //Print SSID name
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  //Print ipv4 assigned to WiFi101 module
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  //Print signal strength for WiFi101 module
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void WiFiInit() {
  //Attempt a WiFi connection to desired access point at ssid, password
  if (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SSIDNAME);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.mode(WIFI_STA);
    while (WiFi.status() != WL_CONNECTED) {
      status = WiFi.begin(SSIDNAME, SSIDPASS);
      Serial.print('.');
      delay(5000); //Wait 5 secs for establishing connection      
    }
    Serial.println("\nConnected.");
    printWifiStatus();  //Print WiFi status
  }
}

void setup() {
  //Configurar pines de entrada y salida
  pinMode(LED, OUTPUT); //LED como salida
  pinMode(WVALVEPWM, OUTPUT); //Potencia de la valvula como salida
  pinMode(WVALVEDIR, OUTPUT); //Direccion de la valvula como salida

  //Limpieza de salidas
  digitalWrite(LED, HIGH);  //Apago LED
  digitalWrite(WVALVEPWM, LOW); //Apago potencia de la válvula
  digitalWrite(WVALVEDIR, LOW); //Fijo sentido de direccion de la valvula

  //Comunicaciones
  Serial.begin(9600); //Comunicaciones seriales para debug con el PC
  MeasInitialize(); //Inicio suavizado
  delay(1000);  //Wait 1 sec for module initialization
  WiFiInit(); //WiFi communications initialization
  ThingSpeak.begin(client);
}

void loop() {
  WiFiInit(); //Reconnect to WiFi
  inputm = smooth(SOIL, totalm, readingsm, readIndexm);
  shumidctrl(); //Activo control de riego
  if (millis() - tprev >= MUESTREO) {
    m = flmap(inputm, ADCAIR, ADCWAT, SMAIR, SMWAT);
    ThingSpeak.setField(1, m);
    ThingSpeak.setField(2, wvalstate);
    //Serial.println("Medicion RAW: " + String(analogRead(SOIL)) + " Medicion Suavizada: " + String(inputm) + " Humedad del suelo(%): " + String(m));
    tprev = millis();
  }
  if (millis() - tprevtx >= TPOST) {
    int x = ThingSpeak.writeFields(CHANNUM, TSKEY)
    if (x == 200) {
      digitalWrite(LED, LOW);
      Serial.println("Channel update successful.");
    }
    else {
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }
    tprevtx = millis();
  }
  digitalWrite(LED, HIGH);
}
