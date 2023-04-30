// Библиотека для создания Wi-Fi подключения (клиент или точка доступа)
#include <ESP8266WiFi.h>
// Библиотека для управления устройством по HTTP (например из браузера)
#include <ESP8266WebServer.h>
// Библиотека для работы с файловой системой
#include <FS.h>
// Библиотека для работы с SPIFFS по FTP
#include <ESP8266FtpServer.h>

// Пин подключения сигнального контакта реле
const byte relay = LED_BUILTIN; 

//Создаем константы имени сети и пароля 
const char *ssid = "MyIntercom";
const char *password = "12345678";

// Задаем флаг статуса соединения 
bool ConnectFlag = false;

// Определяем объект и порт сервера для работы с HTTP
ESP8266WebServer HTTP(80); 
// Определяем объект для работы с модулем по FTP (для отладки HTML)                                             
FtpServer ftpSrv;                                                       


// Объявляем событие на соединение
WiFiEventHandler stationConnectedHandler; 
// Объявляем событие на разъединение
WiFiEventHandler stationDisconnectedHandler; 

void setup() {
  // Инициализируем вывод данных на серийный порт со скоростью 115200 бод
  Serial.begin(115200);
  // Определяем пин реле как исходящий
  pinMode(relay, OUTPUT); 
  // Задаем начальное значение (LOW = 0 - выключено)
  digitalWrite(relay, HIGH); 
  //WiFi.persistent(false); // бережем флеш-память, не перезаписываем данные подключения, если они не изменились с прошлой загрузки
  // Задаем режим работы в качесвте точки доступа
  WiFi.mode(WIFI_AP);
  // Создаем режим подключения по логину и паролю
  WiFi.softAP(ssid, password);
  //Понижаем радиус едйствия сети
  WiFi.setOutputPower(0);
  
  // Инициализируем работу с файловой системой
  SPIFFS.begin();       
  // Инициализируем Web-сервер                                                                          
  HTTP.begin();
  // Поднимаем FTP-сервер для удобства отладки работы HTML (логин: relay, пароль: relay)                                                         
  ftpSrv.begin("relay","relay");                                        

  // Инициализируем событие соединения переход к функции onStationConnected
  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected); 
  // Инициализируем событие соединения переход к функции onStationDisconnected
  stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);

  // Обработка HTTP-запросов
  // При HTTP запросе вида http://192.168.4.1/relay_switch
  HTTP.on("/relay_switch", [](){       
    // Отдаём клиенту код успешной обработки запроса, сообщаем, что формат ответа текстовый и возвращаем результат выполнения функции relay_switch                                  
      HTTP.send(200, "text/plain", relay_switch());                     
  });
  // При HTTP запросе вида http://192.168.4.1/relay_status
  HTTP.on("/relay_status", [](){      
    // Отдаём клиенту код успешной обработки запроса, сообщаем, что формат ответа текстовый и возвращаем результат выполнения функции relay_status                                   
      HTTP.send(200, "text/plain", relay_status());                     
  });
  // Описываем действия при событии "Не найдено"
  HTTP.onNotFound([](){             
  // Если функция handleFileRead (описана ниже) возвращает значение false в ответ на поиск файла в файловой системе                                    
  if(!handleFileRead(HTTP.uri())) 
    // Возвращаем на запрос текстовое сообщение "File isn't found" с кодом 404 (не найдено)                                      
      HTTP.send(404, "text/plain", "Not Found");                        
  });
}

void loop() {
    // Обработчик HTTP-событий (отлавливает HTTP-запросы к устройству и обрабатывает их в соответствии с выше описанным алгоритмом)  
    HTTP.handleClient();     
    // Обработчик FTP-соединений                                           
    ftpSrv.handleFTP();                                                   
}

// Функция включения реле при подключении к сети
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) { 
  //Serial.print("CONNECTED: ");
  digitalWrite(relay, LOW);
  ConnectFlag = true;
  // МАС-адрес в порт
  Serial.println(macToString(evt.mac)); 
}
// Функция выключения реле при отключении от сети
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) { 
  //Serial.print("DISCONNECTED: ");
  digitalWrite(relay, HIGH);
  ConnectFlag = false;
  // МАС-адрес в порт
  Serial.println(macToString(evt.mac)); 
}

// Получаем МАС-адрес клиента в виде строки
String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// Функция переключения реле из веб интерфейса
String relay_switch() {                                                  
  byte state;
  // Если на пине реле высокий уровень  
  if (digitalRead(relay))         
    // то запоминаем, что его надо поменять на низкий                                       
    state = 0;                           
  // иначе                               
  else     
    // запоминаем, что надо поменять на высокий                                                             
    state = 1;   
  // меняем значение на пине подключения реле
  digitalWrite(relay, state);        
  // возвращаем результат, преобразовав число в строку.                                   
  return String(state);                                                 
}

// Функция для определения текущего статуса реле 
String relay_status() {                                                 
  byte state;
  // Если на пине реле высокий уровень 
  if (digitalRead(relay))         
    // то запоминаем его как единицу                                        
    state = 1;    
  // иначе                                                      
  else    
    // запоминаем его как ноль                                                              
    state = 0;  
  // возвращаем результат, преобразовав число в строку                                                        
  return String(state);                                                 
}

// Функция работы с файловой системой
bool handleFileRead(String path){     
  // Если устройство вызывается по корневому адресу, то должен вызываться файл index.html (добавляем его в конец адреса)                                  
  if(path.endsWith("/")) path += "index.html";       
  // С помощью функции getContentType (описана ниже) определяем по типу файла (в адресе обращения) какой заголовок необходимо возвращать по его вызову                   
  String contentType = getContentType(path);       
  // Если в файловой системе существует файл по адресу обращения                     
  if(SPIFFS.exists(path)){       
    // Открываем файл для чтения                                       
    File file = SPIFFS.open(path, "r");   
    // Выводим содержимое файла по HTTP, указывая заголовок типа содержимого contentType                              
    size_t sent = HTTP.streamFile(file, contentType);   
    // Закрываем файл                
    file.close(); 
    // Завершаем выполнение функции, возвращая результатом ее исполнения true (истина)                                                      
    return true;                                                        
  }
  // Завершаем выполнение функции, возвращая результатом ее исполнения false (если не обработалось предыдущее условие)
  return false;                                                         
}

// Функция, возвращающая необходимый заголовок типа содержимого в зависимости от расширения файла
String getContentType(String filename){  
  // Если файл заканчивается на ".html", то возвращаем заголовок "text/html" и завершаем выполнение функции                               
  if (filename.endsWith(".html")) return "text/html";  
  // Если файл заканчивается на ".css", то возвращаем заголовок "text/css" и завершаем выполнение функции                 
  else if (filename.endsWith(".css")) return "text/css"; 
  // Если файл заканчивается на ".js", то возвращаем заголовок "application/javascript" и завершаем выполнение функции               
  else if (filename.endsWith(".js")) return "application/javascript"; 
  // Если файл заканчивается на ".png", то возвращаем заголовок "image/png" и завершаем выполнение функции  
  else if (filename.endsWith(".png")) return "image/png";
  // Если файл заканчивается на ".jpg", то возвращаем заголовок "image/jpg" и завершаем выполнение функции               
  else if (filename.endsWith(".jpg")) return "image/jpeg";           
  // Если файл заканчивается на ".gif", то возвращаем заголовок "image/gif" и завершаем выполнение функции   
  else if (filename.endsWith(".gif")) return "image/gif"; 
  // Если файл заканчивается на ".ico", то возвращаем заголовок "image/x-icon" и завершаем выполнение функции              
  else if (filename.endsWith(".ico")) return "image/x-icon"; 
  // Если ни один из типов файла не совпал, то считаем что содержимое файла текстовое, отдаем соответствующий заголовок и завершаем выполнение функции           
  return "text/plain";                                                  
}
