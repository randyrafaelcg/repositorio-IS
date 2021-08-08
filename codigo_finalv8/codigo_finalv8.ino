#include <LiquidCrystal_I2C.h>
#include "DHTesp.h"
#include <NTPtimeESP.h>
#include "TRIGGER_WIFI.h"
#include "TRIGGER_GOOGLESHEETS.h" 

#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif

#define DEBUG_ON
//t#define tmp 2 //ds18b20 temperatura dentro del compost
#define mzc 0 //mezclador
#define pindht 16 //dht22 control temperatura y humedad
#define btn 14 //boton
#define bmb 12 //bomba de agua
#define moc 13 //moc que activa la ventilacion
#define estado 15 //rele que activa la resistencia de calor

//const int oneWireBus = tmp; 

int lcdColumns = 16;//columnas lcd
int lcdRows = 2; // filas lcd


int hum=0; // humedad dht22
int temp=0;// temperatura dht22
int temprom=0;//temperatura promedio
bool primeravez=false;//la primera vez el contenedor se calentara con el aire caliente, posteriormente se empleara unicamente la resistencia de calor 
int PID_writed=0;
int PID_writed2=0;

char str[16];//cadena para mostrar en lcd
//char estado[16];//estado , mezclando, latencia, termofilico, agua, etc

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  
DHTesp dht;


NTPtime NTPch("br.pool.ntp.org");   // server pool para obtener la hora
char *ssid      = "COMTECO-N3415424";         // WiFi SSID
char *password  = "XSTHI76370";               // WiFi password
char column_name_in_sheets[ ][20] = {"value1","value2","value3"}; 
String Sheets_GAS_ID = "AKfycbz2dIi3fK7GYwxFvWBSvDGJD3s-j08u3BW_uunBagznBvWs6Fc";
int No_of_Parameters = 3;
int enviodatos=0;//enviar datos cada 5 min

int hora=0;//hora actual
int minuto=0;//minuto actual
int segundo=0;//segundo actual
int dia=0;
int mes=0;
int anio=0;
int hrcon=0;//hora control
int mincon=0;//minuto control
int segcon=0;// segundo control
int diacon=0;//dia control
int mintrans=0;//minutos transcurridos desde el inicio del proceso

strDateTime dateTime; // Para la hora

int fase=1; // fase en la que se encuentra el proceso de compostaje. 1-latencia 2-termofilica 3-maduracion
int flat=1;// cantidad de días que se mantendrá en fase de latencia, mantener temperatura superior a 30 grados menor a 65 grados, humedad superior a 20% menor a 70%
int fter=2;// cantidad de días que se mantendrá en fase termofilica, mantener la temperatura entre 45 y 55 grados por al menos 24hrs esto para asegurar eliminar bacterias y/o viruses,pero mantener temperatura menor 70%, humedad entre 50 y 60% menor a 70% 
int fmad=1;// cantidad de días que se mantendrá en fase de maduracion, mantener temperatura superior a 30 grados, humedad superior a 20%  menor a 70%
char modo='u';// modo automatico A, modo manual B, modo indeterminado u
int ciclom=2;// cantidad de veces que se activara el mezclador en 24 hrs dentro la fase termofilica
int aciclom=0;// acumulador para el ciclo de mezclado
int periodom=0;// cada cuantos minutos activar el mezclado
int conthum=2;//contador para activar el sistema de riego una vez cada 5 min si el %hum baja de 30% en fase 1 y 3 y 45% en fase 2
int contluz=8;//apaga la luz luego de 8 min;
bool actventf2=false;//activar ventilacion en fase 2 cada 20 min
int ventf2=0;//tiempo transcurrido para ventilar

int ini=0;//inicio del programa

void setup(){
  pinMode(btn, INPUT);
  pinMode(bmb, OUTPUT);
  pinMode(moc, OUTPUT);
  pinMode(estado, OUTPUT);
  pinMode(mzc, OUTPUT);
  digitalWrite(mzc,LOW);
  digitalWrite(bmb,LOW);
  digitalWrite(estado,LOW);
  //digitalWrite(moc,HIGH);
  //delay(10000);
  digitalWrite(moc,LOW);
  Serial.begin(9600);
  while (!Serial);//ojo
  //attachInterrupt(digitalPinToInterrupt(btn), interrupcion, RISING);
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("BIENVENIDO");
  delay(5000);
  lcd.setCursor(0, 1);
  lcd.print("CONECTANDO WiFi");
  delay(5000);
  WIFI_Connect(ssid,password);
  Google_Sheets_Init(column_name_in_sheets, Sheets_GAS_ID, No_of_Parameters );
  lcd.setCursor(0, 0);
  lcd.print("WiFi conectado");
  delay(2000);
  lcd.clear();
  dht.setup(pindht, DHTesp::DHT22);
}


void interrupcion()//no funciona como interrupcion interna debido a la conexion, se debera desactivar wdt para que funcione, pero podria causar problemas posteriores
{
  if(digitalRead(btn)==HIGH)
  {
    tiempotrans();
    contluz=mintrans+8;
    lcd.backlight();
    lcd.clear();
    while(Serial.available()>0) Serial.read();
    lcd.setCursor(0,0);
    lcd.print("MANTENER PARA");
    lcd.setCursor(0,1);
    lcd.print("ENTRAR AL MENU");  
    digitalWrite(estado,LOW);  
    delay(5000);
    lcd.clear();
    if(digitalRead(btn)==HIGH)
    {
      bool salir=0;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("A:RESET B:PAUSA");
      lcd.setCursor(0,1);
      lcd.print("C:CONTINUAR");
      delay(5000);
      while(salir!=1)
      {
        if (Serial.available())
        {
          char entrada = Serial.read();
          delay(80);
          if(entrada=='A')
          {
            lcd.clear();
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("REINICIANDO");
            delay(5000);
            ESP.restart();
          }
          else if(entrada=='B')
            {
              bool salir1=0;
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("PAUSA");
              lcd.setCursor(0,1);
              lcd.print("#:CONTINUAR");
              delay(50);
              digitalWrite(mzc,LOW);
              digitalWrite(bmb,LOW);
              digitalWrite(estado,LOW);
              digitalWrite(moc,LOW);
              while(salir1!=1)
              {
                if (Serial.available())
                {
                  entrada = Serial.read();
                  delay(80);
                  if(entrada=='#')
                  {
                    salir=1;
                    salir1=1;
                    lcd.clear();
                  }
                 }
              }
            }
            else if(entrada=='C')
            {
              salir=1;
              lcd.clear();
            }
              else
                {
                  lcd.clear();
                  lcd.setCursor(0,0);
                  lcd.print("ERROR");
                  delay(5000);
                  lcd.clear();
                  salir=1;
                }
        }
      }
    }
  }
}

void rutinatiempo()
{
  dateTime = NTPch.getNTPtime(-4, 0);
  if(dateTime.valid){
    
    hora= dateTime.hour;
    minuto=dateTime.minute;
    segundo=dateTime.second;
    dia=dateTime.day;
    mes=dateTime.month;
    anio=dateTime.year;
    delay(20);
  }
}

void rutinasensores()
{
  interrupcion();
  delay(dht.getMinimumSamplingPeriod());
  hum = dht.getHumidity();
  temp = dht.getTemperature();
  if(fase==2)
    {
      if (Serial.available())
      {
        char inByte3 = Serial.read();
        PID_writed=inByte3;
        sprintf(str,"TR:%d ",PID_writed);
        lcd.setCursor(0,1);
        lcd.print(str);
        delay(20);
        while(Serial.available()>0) Serial.read();
      }
      hum=hum+2;
    }
  if(temp>=0 && temp<=90)// verificar un valor valido de temperatura
    temprom=temp;
}

void actualizardatos()
{
  rutinasensores();
  tiempotrans();  
  sprintf(str,"%dC %d%% %d:%d F%d ",temprom,hum,hora,minuto,fase);
  lcd.setCursor(0,0);
  lcd.print(str);
  interrupcion();
}

void tiempotrans()//tiempo transcurrido en min
{
  rutinatiempo();
  if(mincon!=minuto)
  {
    mintrans++;
    mincon=minuto;
    if(conthum>0)
      conthum--;//el sistema de temperatura se activa 1 vez y se hace una pausa
  }
  if(mintrans>=(flat*1440) && mintrans<=((fter*1440)+(flat*1440)))
    fase=2;
    else if(mintrans>((fter*1440)+(flat*1440)) && mintrans<=((fter*1440)+(flat*1440)+(fmad*1440)))
    fase=3;
      else if(mintrans>((fter*1440)+(flat*1440)+(fmad*1440)))
      fase=4;

  if(mintrans>=enviodatos)
  {
    rutinasensores();
    enviodatos=enviodatos+4;
    if(fase==2)
      Data_to_Sheets(No_of_Parameters,  float(mintrans),  float(PID_writed), float(hum));
      else
        Data_to_Sheets(No_of_Parameters,  float(mintrans),  float(temprom), float(hum));
  }

  if(mintrans>contluz)
    lcd.noBacklight();

  if(fase==2 && mintrans>=ventf2)
  {
    ventf2=mintrans+300;
    actventf2=true;
  }
}

void inicio()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("INGRESE EL MODO");
  lcd.setCursor(0,1);
  lcd.print("A:AUTO B:MANUAL");
  delay(80);
  while(modo=='u')
  {
    interrupcion();
    if (Serial.available())
    {
      char inByte = Serial.read();
      delay(80);
      modo=inByte;
      if(modo=='A')
      {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("MODO AUTOMATICO");
        lcd.setCursor(0,1);
        lcd.print("4 DIAS");
        delay(5000);
      }
      else if(modo=='B')
      {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("MODO MANUAL");
        lcd.setCursor(0,1);
        lcd.print("INGRESAR DIAS");
        delay(6000);
        lcd.clear();
        delay(80);
        int cont=1; //contador
        while(cont<=4)
        {
          interrupcion();
          if(cont==1)
          {
            lcd.setCursor(0,0);
            lcd.print("DIAS FASE");
            lcd.setCursor(0,1);
            lcd.print("LATENCIA: ");
            delay(80);
          }
            else if(cont==2)
            {
              lcd.setCursor(0,0);
              lcd.print("DIAS FASE");
              lcd.setCursor(0,1);
              lcd.print("TERMOFILICA:");
              delay(80);
            }
              else if(cont==3)
              {
                lcd.setCursor(0,0);
                lcd.print("DIAS FASE");
                lcd.setCursor(0,1);
                lcd.print("MADURACION:  ");
                delay(80);
              }
                else
                {
                  lcd.setCursor(0,0);
                  lcd.print("MEZCLAR EN 24HR:");
                  delay(80);
                }
            if(Serial.available())
            {
              char inbyte=Serial.read();//inbyte con minuscula para no confundir con el inByte con mayuscula xd
              delay(80);
              if(inbyte=='#')
              {
                cont++;
                lcd.clear();
              }
              else if(inbyte<58 && inbyte>47)
              {
                if(cont==1)
                {
                  lcd.setCursor(10,1);
                  flat=inbyte-48;
                  sprintf(str,"%d ",flat);
                  lcd.print(str);
                }
                else if(cont==2)
                {
                  lcd.setCursor(12,1);
                  fter=inbyte-48;
                  sprintf(str,"%d ",fter);
                  lcd.print(str);                 
                }
                else if(cont==3)
                {
                  lcd.setCursor(13,1);
                  fmad=inbyte-48;
                  sprintf(str,"%d  ",fmad);
                  lcd.print(str);
                }
                else if(cont==4)
                {
                  ciclom=inbyte-48;
                  lcd.setCursor(0,1);
                  sprintf(str,"%d VECES",ciclom);
                  lcd.print(str);
                }
              }
            }
          }
        }
        else
        {
          lcd.clear();
          lcd.setCursor(4,0);
          lcd.print("ERROR");
          lcd.setCursor(0,1);
          lcd.print("INGRESE: A o B");
          delay(5000);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("INGRESE EL MODO");
          lcd.setCursor(0,1);
          lcd.print("A:AUTO B:MANUAL");
          delay(80);
          modo='u';
        }
      }
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("F. LATENCIA:");
    sprintf(str,"%d DIAS",flat);
    lcd.setCursor(0,1);
    lcd.print(str);
    delay(4000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("F. TERMOFILICA:");
    sprintf(str,"%d DIAS",fter);
    lcd.setCursor(0,1);
    lcd.print(str);
    delay(4000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("F. MADURACION:");
    sprintf(str,"%d DIAS",fmad);
    lcd.setCursor(0,1);
    lcd.print(str);
    delay(4000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CICLO MEZCLADO:");
    sprintf(str,"%d VECES en 24hr",ciclom);
    lcd.setCursor(0,1);
    lcd.print(str);
    delay(4000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("INICIO SISTEMA");
    digitalWrite(mzc,HIGH);
    delay(5000);
    digitalWrite(mzc,LOW);
    delay(2000);
    lcd.clear();
    periodom=1440/(ciclom+1);// cada cuantos minutos activar el ciclo
    //periodom=2;//para pruebas
    aciclom=periodom;
    tiempotrans();
    mincon=minuto;
    ini=1;
}

void mezclado() /////////////////////////////////RUTINA PARA EL MEZCLADO///////////////////////////////////////
{
  //if(fase==2 || fase==3)
    if(aciclom<=mintrans)
    {
      int aux=59;
      rutinatiempo();
      while(aux<60 && aux>0)
      {
        interrupcion();
        digitalWrite(mzc,HIGH);
        actualizardatos();
        lcd.setCursor(0,1);
        sprintf(str,"MEZCLANDO %d    ",aux);
        lcd.print(str);
        delay(960);
        aux--;
      }
      aux=59;
      aciclom=aciclom+periodom;//se suma el periodo de mezclado para determinar el siguiente minuto en el cual debera activarse el mezclado
      digitalWrite(mzc,LOW);
      lcd.setCursor(0,1);
      lcd.print("ESTABILIZANDO");
      delay(45000);//delay para que el sensor de humedad se estabilice
      lcd.clear();
      actualizardatos();
    }
}

void controltemp()///////////////////////CONTROL TEMPERATURA POR PID//////////////////////////////////
{
  if(fase!=2)
  {
    digitalWrite(estado,LOW);
    rutinasensores();
    if(temprom<21)
      {
        digitalWrite(estado,HIGH);
        while(temprom<24)
          {              
              actualizardatos();
              digitalWrite(estado,HIGH);
              //lcd.setCursor(0,1);
              //lcd.print("CALENTADOR ");
              interrupcion();
              if (Serial.available())
              {
                char inByte3 = Serial.read();
                PID_writed=inByte3;
                sprintf(str,"TR:%d ",PID_writed);
                lcd.setCursor(0,1);
                lcd.print(str);
                delay(20);
                while(Serial.available()>0) Serial.read();
               }
              rutinasensores();              
            }
        delay(20);
        digitalWrite(estado,LOW);
        while(Serial.available()>0) Serial.read();
        lcd.clear();
        actualizardatos();
        digitalWrite(mzc,HIGH);
        delay(10000);
        digitalWrite(mzc,LOW);
        conthum=5; 
      }    
  }
  else if(fase==2)
  {
    rutinasensores();
    digitalWrite(estado,HIGH);
    if(temp>71)
    {
      lcd.clear();
      while(true)
      {
        digitalWrite(estado,LOW);
        lcd.setCursor(0,1);
        lcd.print("ERROR FATAL");
        delay(5000);
        while(Serial.available()>0) Serial.read();
      }
    }
  }
}

void controlhum()/////////////////////////CONTROL DE HUMEDAD ON OFF////////////////////////////////////////////
{
  rutinasensores();
  if(hum>65 && fase!=2)
  {
    while(hum>56)
    {
      actualizardatos();
      digitalWrite(moc,HIGH);
      lcd.setCursor(8,1);
      lcd.print("VON");
      rutinasensores();
    }
    digitalWrite(moc,LOW);
    lcd.clear();
    actualizardatos();
    //digitalWrite(mzc,HIGH);
    //delay(10000);
    digitalWrite(mzc,LOW);
    conthum=1;
  }
  else if(hum>60 && fase==2)
  {
    while(hum>54)
    {
      actualizardatos();
      digitalWrite(moc,HIGH);
      lcd.setCursor(8,1);
      lcd.print("VON");
      delay(2000);
      rutinasensores();
    }
    digitalWrite(moc,LOW);
    delay(2000);
    lcd.clear();
    actualizardatos();
    digitalWrite(mzc,LOW);
    conthum=4;
    ventf2=ventf2+30;
  }
  if(actventf2 && hum>52)
  {
    actventf2=false;
    digitalWrite(moc,HIGH);
    lcd.setCursor(8,1);
    lcd.print("VON f2");
    delay(5000);
    actualizardatos();
    digitalWrite(moc,LOW);
    lcd.clear();
    conthum=8;
  }
  if(fase==1 || fase==3)
  {
    rutinasensores();
    if(hum<45 && conthum<=0)
    {
      actualizardatos();
      lcd.setCursor(0,1);
      lcd.print("SIST RIEGO ACT");
      delay(3000);
      digitalWrite(bmb,HIGH);
      delay(3000);
      digitalWrite(bmb,LOW);
      conthum=6;
      lcd.clear();
    }
  }
    rutinasensores();
    if(hum<30 && conthum<=0 && fase==2)
    {
      actualizardatos();
      lcd.setCursor(0,1);
      lcd.print("SIST RIEGO ACT");
      delay(3000);
      digitalWrite(bmb,HIGH);
      delay(2000);
      digitalWrite(bmb,LOW);
      conthum=5;
      lcd.clear();
    }
}


////////////////PROGRAMA PRINCIPAL/////////////////////////////////
void loop()
{
  if(ini==0)
    inicio();  
  actualizardatos();
  mezclado();
  controltemp();
  controlhum();
  interrupcion();
  if(fase==4)
  {
    lcd.clear();
    while(true)
    {
      actualizardatos();
      controlhum();
      digitalWrite(mzc,LOW);
      digitalWrite(bmb,LOW);
      digitalWrite(estado,LOW);
      digitalWrite(moc,LOW);
      lcd.setCursor(0,1);
      lcd.print("FINALIZADO");
      interrupcion();
    }
  }
}
